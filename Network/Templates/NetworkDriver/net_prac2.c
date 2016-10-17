#include <linux/version.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/net.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include "net_prac.h"

#define NUM_TX_DESC	4
#define DRV_NAME        "rt8139"
#define RT_VENDOR       0X10EC
#define RT_DEVICE       0X8136
#define TX_TIMEOUT      (6 * HZ) 

#define RX_BUF_LEN_IDX	2
#define RX_BUF_LEN	(8192 << RX_BUF_LEN_IDX)
#define RX_BUF_PAD	16
#define RX_BUF_WRAP_PAD	2048
#define RX_BUF_TOT_LEN	(RX_BUF_LEN + RX_BUF_PAD + RX_BUF_WRAP_PAD)

struct rt8139_priv {
	struct pci_dev *pci_dev;
	void __iomem *mmio_addr;
	unsigned long regs_len;
	unsigned int tx_flag;
	unsigned int cur_tx;
	unsigned int dirty_tx;
	unsigned char *tx_buf[NUM_TX_DESC];
	unsigned char *tx_bufs;
	dma_addr_t tx_bufs_dma;

	struct net_device_stats stats;
	unsigned char *rx_ring;
	dma_addr_t rx_ring_dma;
	unsigned int cur_rx;
};


static struct rt8139_init(struct pci_device *pdev)
{
	void __iomem *ioaddr;
	unsigned long mmio_start, mmio_end, mmio_len, mmio_flags;
	struct net_device *dev;
	struct rt8139_priv *tp;
	int ec;

	dev = alloc_etherdev(sizeof(*tp));
	if (dev == NULL) {
		dev_err(&pdev->dev, "Unable the allocate the etherdev\n");
		return ERR_PTR(-ENOMEM);
	}
	SET_NETDEV_DEV(dev, &pdev->dev);
	tp = netdev_priv(dev);
	tp->pci_dev = pdev;
	ec = pci_enable_device(pdev);
	if (ec) {
		dev_err(&pdev->dev, "Unable to enable the device\n");
		goto err_out;
	}
	mmio_start = pci_resource_start(pdev, 1);
	mmio_end = pci_resource_end(pdev, 1);
	mmio_len = pci_resource_end(pdev, 1);
	mmio_flags = pci_resource_flags(pdev, 1);

	if (!(mmio_flags & IORESOURCE_MEM)) {
		dev_err(&pdev->dev, "Unable to find the mapped memory\n");
		ec = -ENODEV;
		goto err_out;
	}

	ec = pci_request_regions(pdev, DRV_NAME);
	if (ec) {
		dev_err(&pdev->dev, "Unable to request the regions\n");
		return ERR_PTR(ec);
	}
	pci_set_master(pdev);

	ioaddr = pci_iomap(pdev, 1, 0);
	if (ioaddr == NULL) {
		dev_err(&pdev->dev, "Unable to map io\n");
		pci_release_regions(pdev);
		goto err_out;
	}
	dev->base_addr = (long)ioaddr;
	tp->regs_len = mmio_len;
	tp->mmio_addr = ioaddr;

	/*Do some chip related stuff */

	return dev;

err_out:
	rt8139_cleanup(dev);
	return ERR_PTR(ec);
}

static void rt8139_chip_reset(void *ioaddr)
{
	int i;
	writeb(CmdReset, (ioaddr + CR));
	/* Check that the chip has finished the reset */
	for (i = 1000; i > 0; i--) {
		barrier();
		if (readb(ioaddr + CR) & CmdReset) break;
		udelay(10);
	}
	return;
}

static void rt8139_init_hw(struct net_device *dev)
{
	struct rt8139_priv *tp = netdev_priv(dev);
	void __iomem *ioaddr;
	ioaddr = tp->mmio_addr;
	uint32_t i;
	rt8139_chip_reset(ioaddr);
	/* Enable the TX */
	writeb(CmdTxEnb | CmdRxEnb, ioaddr + CR);
	/* Tx config */
	writel(0X600, ioaddr + CR);
	/* Rx Config */
	writel((1 << 12) | (7 << 8) | (1 << 7) | (1 << 3) | (1 << 2) | (1 << 1), ioaddr + RCR);
	for (i = 0; i < NUM_TX_DESC; i++)
		writel(tp->tx_bufs_dma + (tp->tx_buf[i] - tp->tx_bufs), ioaddr + (TSAD0 + i * 4));
	/*init RBSTART */
	writel(tp->rx_ring_dma, ioaddr + RBSTART);
	
	/* init Missed packet counter */
	writel(0, ioaddr + MPC);

	/* Enable all the interrupts */
	writew(INT_MASK, ioaddr + IMR);
	netif_start_queue(dev);
	return;
}

void rt8139_init_ring(struct net_device *dev)
{
	int i;
	struct rt8139_priv *tp = netdev_priv(dev);
	tp->cur_tx = 0;
	tp->cur_rx = 0;
	tp->dirty_tx= 0;

	for (i = 0; i < NUM_TX_DESC; i++)
	{
		tp->tx_buf[i] = &tp->tx_buffs[i * TX_BUFF_SIZE];
	}
	return;
}

static int rt8139_open(struct net_device *dev)
{
	dev_info(&dev->dev, "In open\n");
	int retval;
	struct rt8139_priv *tp;
	tp = netdev_priv(dev);
	if (retval = request_irq(dev->irq, rt8139_hand, 0, dev->name, dev))
		return retval;
	tp->tx_buffs = pci_alloc_consistent(tp->pci_dev, TOTAL_TX_BUF_SIZE, &tp->tx_bufs_dma);

	tp->rx_ring = pci_alloc_consistent(tp->pci_dev, RX_BUF_TOT_LEN, &tx->rx_ring_dma);

	if (tp->tx_buffs == NULL || tp->rx_ring == NULL) {
		dev_err(&dev->dev, "Unable to allocate the buffers\n");
		free_irq(dev->irq);
		if (tp->tx_buffs) {
			pci_free_consistent(tp->pci_dev, TOTAL_TX_BUF_SIZE, tp->tx_buffs, tp->tx_bufs_dma);
			tp->tx_buffs = NULL;
		}
		if (tp->rx_ring) {
			pci_free_consistent(tp->pci_dev, RX_BUF_TOT_LEN, tp->rx_ring, tp->rx_ring_dma);
			tp->rx_ring = NULL;
		}
		return -ENOMEM;
	}
	tp->tx_flag = 0;
	rt8139_init_ring(dev);
	rt8139_init_hw(dev);
	return 0;
}

static int rt8139_close(struct net_device *dev)
{
	dev_info(&dev->dev, "In close\n");
	return 0;
}

static netdev_tx_t rt8139_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct rt8139_priv *tp = netdev_priv(dev);
	int len = skb->len;
	int entry = tp->cur_tx;
	dev_info(&dev->dev, "In xmit\n");

	if (len < TX_BUFF_SIZE) {
		if (len < ETH_MIN_LEN) memset(tp->tx_buf[entry], ETH_MIN_LEN, 0);
		skb_copy_and_csum_dev(skb, tp->tx_buf[entry]);
		dev_kfree_skb(skb);
	}
	else {
		dev_kfree_skb(skb);
		return 0;
	}
	writel(tp->tx_flag | max(len, (uint32_t)ETH_MIN_LEN), 
			ioaddr + TSD0 + (entry * sizeof(int)));
	entry++;
	tp->curr_tx = entry % NUM_TX_DESC;
	if (tp->cur_tx == tp->dirty_tx) {
		netif_stop_queue(dev);
	}
	return 0;
}

static struct net_device_stats *rt8139_get_stats(struct net_device *dev)
{
	dev_info(&dev->dev, "In xmit\n");
	return 0;
}

static const struct net_device_ops rt8139_netdev_ops = {
	.ndo_open = rt8139_open,
	.ndo_stop = rt8139_close,
	.ndo_get_stats = rt8139_get_stats,
	.ndo_start_xmit = rt8139_start_xmit,
	//.ndo_set_rx_mode = rt813
};

static int __devinit rt8139_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	void __iomem *ioaddr;
	struct net_device *dev = NULL;
	struct rt8139_priv *tp;
	int i;

	dev = rt8139_init(pdev);
	if (IS_ERR(dev))
		return PTR_ERR(dev);
	/*Read the hw addr_len from eeprom */
	/* This is being read from the eeprom in the new driver */
	/* UPDATE NET_DEVICE */
	   for (i = 0; i < 6; i++) {  /* Hardware Address */
		         dev->dev_addr[i] = readb((void *)(dev->base_addr+i));
			       dev->broadcast[i] = 0xff;
			          }
	dev->hard_header_len = 14;
	dev->netdev_ops = &rt8139_netdev_ops;
	//dev->ethtool_ops = &rt8139_ethtool_ops;
	dev->watchdog_timeo = TX_TIMEOUT;
	memcpy(dev->name, DRV_NAME, sizeof(DRV_NAME));
	dev->irq = pdev->irq;
	/* Register the device */
	if (register_netdev(dev)) {
		dev_err(&pdev->dev, "Registration failed\n");
		goto clean_up;
	}
	/* So with respect to new driver, following things are left out 
	 * 1. Workque:ue initialization
	 * 2. spin lock initialiation for tp->lock, tp->rx_lock
	 * 3. adding the napi
	 * 4. Adding the features such as Scatter gather, CSUM and so on
	 * 5. Adding the mii register details, mdio read and write bus
	 */
	pci_set_drvdata(pdev, dev);
	return 0;
	

clean_up:
	rt8139_cleanup(dev);
	return -1;
}

}

static struct pci_device_id rt8139_table[] = {
	{PCI_DEVICE(RT_VENDOR, RT_DEVICE)},
	{},
};

MODULE_DEVICE_TABLE(pci, rt8139_table);

static struct pci_driver rt8139_pci = {
	.name = DRV_NAME,
	.probe = rt8139_probe,
	.remove = rt8139_remove,
	.id_table = rt8139_table,
};
int __init init_module(void)
{
	return pci_register_driver(&rt8139_pci);
}

void __exit cleanup_module(void)
{
	pci_unregister_driver(&rt8139_pci);
}
