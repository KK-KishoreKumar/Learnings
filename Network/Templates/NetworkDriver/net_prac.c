#include <linux/version.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/net.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include "net_prac.h"

#define DRV_NAME	"rt8139"
#define RT_VENDOR	0X10EC
#define RT_DEVICE	0X8136
#define TX_TIMEOUT	(6 * HZ)

struct rt8139_priv {
	struct pci_dev *pci_dev;
	void __iomem *mmio_addr;
	unsigned long regs_len;
};


static void rt8139_cleanup(struct net_device *dev)
{
	struct rt8139_priv *tp = netdev_priv(dev);
	struct pci_dev *pdev;

	//assert (dev != NULL);
	//assert (tp->pci_dev != NULL);
	pdev = tp->pci_dev;

	if (tp->mmio_addr)
		pci_iounmap (pdev, tp->mmio_addr);

	/* it's ok to call this even if we have no regions to free */
	pci_release_regions (pdev);

	free_netdev(dev);
} 

static struct net_device *rt8139_init(struct pci_dev *pdev)
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

static int rt8139_open(struct net_device *dev)
{
	dev_info(&dev->dev, "In open\n");
	return 0;
}

static int rt8139_close(struct net_device *dev)
{
	dev_info(&dev->dev, "In close\n");
	return 0;
}

static netdev_tx_t rt8139_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	dev_info(&dev->dev, "In xmit\n");
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

static int __devinit rt8139_probe (struct pci_dev *pdev,
		                                       const struct pci_device_id *ent)
//static int rt8139_probe(struct pci_dev *pdev, struct pci_device_id *id)
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
	

clean_up:
	rt8139_cleanup(dev);
	return -1;
}

static void __devexit rt8139_remove (struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata (pdev);
	struct rtl8139_private *tp = netdev_priv(dev);

	//assert (dev != NULL);

	//cancel_delayed_work_sync(&tp->thread);

	unregister_netdev (dev);

	rt8139_cleanup(dev);
	pci_disable_device (pdev);
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
