#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/stddef.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/in.h>
#include <linux/skbuff.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/workqueue.h>
#include <linux/timer.h>

#include "rt8139.h"

#define DRIVER "rtl8139"
#define REALTEK_VENDOR_ID  0x10EC
#define REALTEK_DEVICE_ID  0x8136

/* Size of the in-memory receive ring. */
#define RX_BUF_LEN_IDX 2         /* 0==8K, 1==16K, 2==32K, 3==64K */
#define RX_BUF_LEN     (8192 << RX_BUF_LEN_IDX)
#define RX_BUF_PAD     16           /* see 11th and 12th bit of RCR: 0x44 */
#define RX_BUF_WRAP_PAD 2048   /* spare padding to handle pkt wrap */
#define RX_BUF_TOT_LEN  (RX_BUF_LEN + RX_BUF_PAD + RX_BUF_WRAP_PAD)

#define ETH_MIN_LEN 60  /* minimum Ethernet frame size */

/* write MMIO register, with flush */
		/* Flush avoids rtl8139 bug w/ posted MMIO writes */
#define RTL_W8_F(reg, val8)     do { iowrite8 ((val8), ioaddr + (reg)); ioread8 (ioaddr + (reg)); } while (0)
#define RTL_W16_F(reg, val16)   do { iowrite16 ((val16), ioaddr + (reg)); ioread16 (ioaddr + (reg)); } while (0)
#define RTL_W32_F(reg, val32)   do { iowrite32 ((val32), ioaddr + (reg)); ioread32 (ioaddr + (reg)); } while (0)

		/* write MMIO register */
#define RTL_W8(reg, val8)       iowrite8 ((val8), ioaddr + (reg))
#define RTL_W16(reg, val16)     iowrite16 ((val16), ioaddr + (reg))
#define RTL_W32(reg, val32)     iowrite32 ((val32), ioaddr + (reg))

		/* read MMIO register */
#define RTL_R8(reg)             ioread8 (ioaddr + (reg))
#define RTL_R16(reg)            ioread16 (ioaddr + (reg))
#define RTL_R32(reg)            ioread32 (ioaddr + (reg))

enum RTL8139_registers {
	MAC0            = 0,     /* Ethernet hardware address. */
	MAR0            = 8,     /* Multicast filter. */
	TxStatus0       = 0x10,  /* Transmit status (Four 32bit registers). */
	TxAddr0         = 0x20,  /* Tx descriptors (also four 32bit). */
	RxBuf           = 0x30,
	ChipCmd         = 0x37,
	RxBufPtr        = 0x38,
	RxBufAddr       = 0x3A,
	IntrMask        = 0x3C,
	IntrStatus      = 0x3E,
	TxConfig        = 0x40,
	RxConfig        = 0x44,
	Timer           = 0x48,  /* A general-purpose counter. */
	RxMissed        = 0x4C,  /* 24 bits valid, write clears. */
	Cfg9346         = 0x50,
	Config0         = 0x51,
	Config1         = 0x52,
	TimerInt        = 0x54,
	MediaStatus     = 0x58,
	Config3         = 0x59,
	Config4         = 0x5A,  /* absent on RTL-8139A */
	HltClk          = 0x5B,
	MultiIntr       = 0x5C,
	TxSummary       = 0x60,
	BasicModeCtrl   = 0x62,
	BasicModeStatus = 0x64,
	NWayAdvert      = 0x66,
	NWayLPAR        = 0x68,
	NWayExpansion   = 0x6A,
	/* Undocumented registers, but required for proper operation. */
	FIFOTMS         = 0x70,  /* FIFO Control and test. */
	CSCR            = 0x74,  /* Chip Status and Configuration Register. */
	PARA78          = 0x78,
	FlashReg        = 0xD4, /* Communication with Flash ROM, four bytes. */
	PARA7c          = 0x7c,  /* Magic transceiver parameter register. */
	Config5         = 0xD8,  /* absent on RTL-8139A */
};


static struct net_device *rtl8139_dev;

struct rtl8139_private {
	struct pci_dev *pci_dev;             /* PCI device */
	void *mmio_addr;                     /* memory mapped I/O addr */
	unsigned long regs_len;              /* length of I/O or MMI/O region */
	unsigned int tx_flag;
	unsigned int cur_tx;
	unsigned int dirty_tx;
	unsigned char *tx_buf[NUM_TX_DESC];  /* Tx bounce buffers */
	unsigned char *tx_bufs;              /* Tx bounce buffer region. */
	dma_addr_t tx_bufs_dma;

	struct net_device_stats stats;
	unsigned char *rx_ring;
	dma_addr_t rx_ring_dma;
	unsigned int cur_rx;
};

static irqreturn_t rtl8139_interrupt (int irq, void *dev_instance) {
	struct net_device *dev = (struct net_device*)dev_instance;
	struct rtl8139_private *tp = netdev_priv(dev);
	void *ioaddr = tp->mmio_addr;
	unsigned short isr = readw(ioaddr + ISR);

	/* clear all interrupt.
	 *     * Specs says reading ISR clears all interrupts and writing
	 *         * has no effect. But this does not seem to be case. I keep on
	 *             * getting interrupt unless I forcibly clears all interrupt :-(
	 *                 */
	writew(0xffff, ioaddr + ISR);

	if ((isr & TxOK) || (isr & TxErr)) {
		while ((tp->dirty_tx != tp->cur_tx) || netif_queue_stopped(dev)) {
			unsigned int txstatus = readl(ioaddr + TSD0 + tp->dirty_tx * sizeof(int));

			/* not yet transmitted */
			if (!(txstatus & (TxStatOK | TxAborted | TxUnderrun))) break;

			if (txstatus & TxStatOK) {
				printk("Transmit OK interrupt\n");
				tp->stats.tx_bytes += (txstatus & 0x1fff);
				tp->stats.tx_packets++;
			} else {
				printk("Transmit Error interrupt\n");
				tp->stats.tx_errors++;
			}

			tp->dirty_tx++;
			tp->dirty_tx = tp->dirty_tx % NUM_TX_DESC;

			if ((tp->dirty_tx == tp->cur_tx) & netif_queue_stopped(dev)) {
				printk("waking up queue\n");
				netif_wake_queue(dev);
			}
		}
	}

	if (isr & RxErr) {
		/* TODO: Need detailed analysis of error status */
		printk("receive err interrupt\n");
		tp->stats.rx_errors++;
	}

	if (isr & RxOK) {
		printk("receive interrupt received\n");
		while((readb(ioaddr + CR) & RxBufEmpty) == 0)  {
			unsigned int rx_status;
			unsigned short rx_size;
			unsigned short pkt_size;
			struct sk_buff *skb;

			if (tp->cur_rx > RX_BUF_LEN)
				tp->cur_rx = tp->cur_rx % RX_BUF_LEN;

			/* TODO: need to convert rx_status from little to host endian
			 *           * XXX: My CPU is little endian only :-)  */
			rx_status = *(unsigned int*)(tp->rx_ring + tp->cur_rx);
			rx_size = rx_status >> 16;

			/* first two bytes are receive status register 
			 *           * and next two bytes are frame length  */
			pkt_size = rx_size - 4;

			/* hand over packet to system */
			if ((skb = dev_alloc_skb (pkt_size + 2))) {
				skb->dev = dev;
				skb_reserve (skb, 2); /* 16 byte align the IP fields */

				//eth_copy_and_sum(skb, tp->rx_ring + tp->cur_rx + 4, pkt_size, 0);
				skb_copy_to_linear_data (skb, tp->rx_ring + tp->cur_rx + 4, pkt_size);

				skb_put (skb, pkt_size);
				skb->protocol = eth_type_trans (skb, dev);
				netif_rx (skb);

				dev->last_rx = jiffies;
				tp->stats.rx_bytes += pkt_size;
				tp->stats.rx_packets++;
			} else {
				printk("Memory squeeze, dropping packet.\n");
				tp->stats.rx_dropped++;
			}

			/* update tp->cur_rx to next writing location  * /
			 *          tp->cur_rx = (tp->cur_rx + rx_size + 4 + 3) & ~3;
			 *
			 *                   update CAPR */
			writew(tp->cur_rx, ioaddr + CAPR);
		}
	}

	if (isr & CableLen) printk("cable length change interrupt\n");
	if (isr & TimeOut) printk("time interrupt\n");
	if(isr & SysErr) printk("system err interrupt\n");
	return IRQ_HANDLED;
}



static struct pci_dev* probe_for_realtek8139(void) {
	struct pci_dev *pdev = NULL;

	/* Look for RealTek 8139 NIC */
	if ((pdev = pci_get_device(REALTEK_VENDOR_ID, REALTEK_DEVICE_ID, NULL))) {
		/* device found, enable it */
		if (pci_enable_device(pdev)) {
			printk("Could not enable the device\n");
			return NULL;
		} else
			printk("Device enabled\n");
	} else {
		printk("Device not found\n");
		return pdev;
	}
	return pdev;
}


static int rtl8139_start_xmit(struct sk_buff *skb, struct net_device *dev) {
	struct rtl8139_private *tp = netdev_priv(dev);
	void *ioaddr = tp->mmio_addr;
	unsigned int entry = tp->cur_tx;
	unsigned int len = skb->len;

	if (len < TX_BUF_SIZE) {
		if (len < ETH_MIN_LEN) memset(tp->tx_buf[entry], 0, ETH_MIN_LEN);
		skb_copy_and_csum_dev(skb, tp->tx_buf[entry]);
		dev_kfree_skb(skb);
	} else {
		dev_kfree_skb(skb);
		return 0;
	}
	writel(tp->tx_flag | max(len, (unsigned int)ETH_MIN_LEN), 
			ioaddr + TSD0 + (entry * sizeof (u32)));
	entry++;
	tp->cur_tx = entry % NUM_TX_DESC;

	if (tp->cur_tx == tp->dirty_tx) {
		netif_stop_queue(dev);
	}
	return 0;
}

static struct net_device_stats* rtl8139_get_stats(struct net_device *dev) {
	struct rtl8139_private *tp = netdev_priv(dev);
	return &(tp->stats);
}

static inline void rtl8139_tx_clear (struct rtl8139_private *tp)
{
	tp->cur_tx = 0;
	tp->dirty_tx = 0;

	/* XXX account for unsent Tx packets in tp->stats.tx_dropped */
}


static int rtl8139_stop(struct net_device *dev) {
	
	struct rtl8139_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;

	printk("rtl8139_stop is called\n");

	netif_stop_queue(dev);
	   /* Stop the chip's Tx and Rx DMA processes. */
	        RTL_W8 (ChipCmd, 0);

		        /* Disable interrupts by clearing the interrupt mask. */
		        RTL_W16 (IntrMask, 0);
	 free_irq (dev->irq, dev);
	 rtl8139_tx_clear (tp);
	 tp->rx_ring = NULL;
	 tp->tx_bufs = NULL;

	return 0;
}

static void rtl8139_chip_reset (void *ioaddr) {
	int i;

	/* Soft reset the chip. */
	writeb(CmdReset, ioaddr + CR);

	/* Check that the chip has finished the reset. */
	for (i = 1000; i > 0; i--) {
		barrier();
		if ((readb(ioaddr + CR) & CmdReset) == 0) break;
		udelay (10);
	}
	return;
}

static void rtl8139_hw_start (struct net_device *dev) {
	struct rtl8139_private *tp = netdev_priv(dev);
	void *ioaddr = tp->mmio_addr;
	u32 i;

	rtl8139_chip_reset(ioaddr);

	/* Must enable Tx/Rx before setting transfer thresholds! */
	writeb(CmdTxEnb | CmdRxEnb, ioaddr + CR);

	/* tx config */
	writel(0x00000600, ioaddr + TCR); /* DMA burst size 1024 */

	/* rx config */
	writel(((1 << 12) | (7 << 8) | (1 << 7) | 
				(1 << 3) | (1 << 2) | (1 << 1)), ioaddr + RCR);

	/* init Tx buffer DMA addresses */
	for (i = 0; i < NUM_TX_DESC; i++) {
		writel(tp->tx_bufs_dma + (tp->tx_buf[i] - tp->tx_bufs),
				ioaddr + TSAD0 + (i * 4));
	}

	/* init RBSTART */
	writel(tp->rx_ring_dma, ioaddr + RBSTART);

	/* initialize missed packet counter */
	writel(0, ioaddr + MPC);

	/* no early-rx interrupts */
	writew((readw(ioaddr + MULINT) & 0xF000), ioaddr + MULINT);

	/* Enable all known interrupts by setting the interrupt mask. */
	writew(INT_MASK, ioaddr + IMR);

	netif_start_queue (dev);
	return;
}

static void rtl8139_init_ring (struct net_device *dev) {
	struct rtl8139_private *tp = netdev_priv(dev);
	int i;

	tp->cur_tx = 0;
	tp->dirty_tx = 0;

	for (i = 0; i < NUM_TX_DESC; i++)
		tp->tx_buf[i] = &tp->tx_bufs[i * TX_BUF_SIZE];

	return;
}

static int rtl8139_open(struct net_device *dev) {
	int retval;
	struct rtl8139_private *tp = netdev_priv(dev);

	/* get the IRQ
	 *     * second arg is interrupt handler
	 *         * third is flags, 0 means no IRQ sharing  */
	if ((retval=request_irq(dev->irq, rtl8139_interrupt, 0, dev->name, dev)))
		return retval;

	/* get memory for Tx buffers
	 *     * memory must be DMAable  */
	tp->tx_bufs = 
		pci_alloc_consistent(tp->pci_dev, TOTAL_TX_BUF_SIZE, &tp->tx_bufs_dma);

	tp->rx_ring = pci_alloc_consistent(tp->pci_dev, RX_BUF_TOT_LEN, &tp->rx_ring_dma);

	if ((!tp->tx_bufs)  || (!tp->rx_ring)) {
		free_irq(dev->irq, dev);

		if (tp->tx_bufs) {
			pci_free_consistent(tp->pci_dev, TOTAL_TX_BUF_SIZE, tp->tx_bufs, tp->tx_bufs_dma);
			tp->tx_bufs = NULL;
		}
		if (tp->rx_ring) {
			pci_free_consistent(tp->pci_dev, RX_BUF_TOT_LEN, tp->rx_ring, tp->rx_ring_dma);
			tp->rx_ring = NULL;
		}
		return -ENOMEM;
	}

	tp->tx_flag = 0;
	rtl8139_init_ring(dev);
	rtl8139_hw_start(dev);

	//Receive related

	return 0;
}

static const struct net_device_ops rtl8139_device_ops = {
	.ndo_open = rtl8139_open,
	.ndo_stop = rtl8139_stop,
	.ndo_start_xmit = rtl8139_start_xmit,
	.ndo_get_stats = rtl8139_get_stats,
};

static int rtl8139_init(struct pci_dev *pdev, struct net_device **dev_out) {
	struct net_device *dev;
	struct rtl8139_private *tp;

	/* 
	 *     * alloc_etherdev allocates memory for dev and the user's private struct
	 *         * which has sizeof(struct rtl8139_private) memory allocated.  */
	if (!(dev = alloc_etherdev(sizeof(struct rtl8139_private)))) {
		printk("Could not allocate etherdev\n");
		return -1;
	}

	tp = netdev_priv(dev);
	tp->pci_dev = pdev;
	*dev_out = dev;

	return 0;
}

int init_module(void) {

	struct pci_dev *pdev;
	unsigned long mmio_start, mmio_end, mmio_len, mmio_flags;
	void *ioaddr;
	struct rtl8139_private *tp;
	int i;

	if (!(pdev = probe_for_realtek8139( ))) return -1;

	if (rtl8139_init(pdev, &rtl8139_dev)) {
		printk("Could not initialize device\n");
		return -1;
	}

	tp = netdev_priv(rtl8139_dev);     /* rtl8139 private information */

	/* get PCI memory mapped I/O space base address from BAR1 */
	mmio_start = pci_resource_start(pdev, 2);
	mmio_end = pci_resource_end(pdev, 2);
	mmio_len = pci_resource_len(pdev, 2);
	mmio_flags = pci_resource_flags(pdev, 2);

	/* make sure above region is MMI/O */
	if (!(mmio_flags & IORESOURCE_MEM)) {
		printk("region not MMIO region\n");
		goto cleanup1;
	}

	/* get PCI memory space */
	if (pci_request_regions(pdev, DRIVER)) {
		printk("Could not get PCI region\n");
		goto cleanup1;
	}

	/* Enable bus mastering of the PCI device to enable the device
	 *       to initiate transactions  */
	pci_set_master(pdev);

	/* ioremap MMI/O region */
	if (!(ioaddr = ioremap(mmio_start, mmio_len))) {
		printk("Could not ioremap\n");
		goto cleanup2;
	}

	rtl8139_dev->base_addr = (long)ioaddr;
	tp->mmio_addr = ioaddr;
	tp->regs_len = mmio_len;

	/* UPDATE NET_DEVICE */
	for (i = 0; i < 6; i++) {  /* Hardware Address */
		rtl8139_dev->dev_addr[i] = readb((void __iomem *)rtl8139_dev->base_addr+i);
		rtl8139_dev->broadcast[i] = 0xff;
	}
	rtl8139_dev->hard_header_len = 14;

	memcpy(rtl8139_dev->name, DRIVER, sizeof(DRIVER)); /* Device Name */
	rtl8139_dev->irq = pdev->irq;              /* Interrupt Number */
	rtl8139_dev->netdev_ops = &rtl8139_device_ops;

	/* register the device */
	if (register_netdev(rtl8139_dev)) {
		printk("Could not register netdevice\n");
		goto cleanup0;
	}
	return 0;

cleanup0:
	iounmap(tp->mmio_addr);
cleanup2:
	pci_release_regions(tp->pci_dev);
cleanup1:
	pci_disable_device(tp->pci_dev);
	return -1;

}

void cleanup_module(void) {
	struct rtl8139_private *tp;
	tp = netdev_priv(rtl8139_dev);

	iounmap(tp->mmio_addr);
	pci_release_regions(tp->pci_dev);

	unregister_netdev(rtl8139_dev);
	pci_disable_device(tp->pci_dev);
	return;
}

//module_init(init_module);
//module_exit(cleanup_module);
