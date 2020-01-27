#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/nfc.h>
#include <linux/netdevice.h>
#include <net/nfc/nfc.h>

#include "pn533.h"

#define VERSION "0.1"

#define PN533_SPI_DRIVER_NAME "pn533_spi"


// &spi0 {
//     status = "okay";                               
//     max-freq = <24000000>;
//     spidev@00 { 
//         compatible = "nxp,pn533-spi";
//         reg = <0x00>;
//         spi-max-frequency = <14000000>;
//         spi-cpha = <1>;
//         //spi-cpol = <1>;
//     };
// };

struct pn533_spi_phy {
    struct spi_device *spi_dev;
    struct pn533 *priv;

    bool aborted;

    int hard_fault;     /*
                 * < 0 if hardware error occurred (e.g. i2c err)
                 * and prevents normal operation.
                 */
};

static int pn533_spi_send_frame(struct pn533 *dev,
                struct sk_buff *out)
{
    struct pn533_spi_phy *phy = dev->phy;
    struct spi_client *client = phy->i2c_dev;
    int rc;

    if (phy->hard_fault != 0)
        return phy->hard_fault;

    if (phy->priv == NULL)
        phy->priv = dev;

    phy->aborted = false;

    print_hex_dump_debug("PN533_i2c TX: ", DUMP_PREFIX_NONE, 16, 1,
                 out->data, out->len, false);

    rc = spy_write(de, out->data, out->len);

    if (rc == -EREMOTEIO) { /* Retry, chip was in power down */
        usleep_range(6000, 10000);
        rc = i2c_master_send(client, out->data, out->len);
    }

    if (rc >= 0) {
        if (rc != out->len)
            rc = -EREMOTEIO;
        else
            rc = 0;
    }

    return rc;
}

static const struct of_device_id of_pn533_spi_match[] = {
    { .compatible = "nxp,pn533-spi" },
    {},
};

MODULE_DEVICE_TABLE(of, of_pn533_spi_match);

static struct spi_driver pn533_spi_driver = {
    .driver = {
        .name =         PN533_SPI_DRIVER_NAME,
        .owner =        THIS_MODULE,
        .of_match_table = of_match_ptr(of_pn533_spi_match),
     },
	.probe = pn533_spi_probe,
	.remove = pn533_spi_remove,
};

module_spi_driver(pn533_spi_driver);














static int pn533_spi_probe(struct spi_device *client)
{
	struct pn533 *priv;
    struct pn533_spi_phy *phy;
   

    printk("===============spi_pn533_probe ==============\n");


    if(!spi)	
        return -ENOMEM;

    dev_dbg(&client->dev, "%s\n", __func__);
    dev_dbg(&client->dev, "IRQ: %d\n", client->irq);



    phy = devm_kzalloc(&client->dev, sizeof(struct pn533_spi_phy),
               GFP_KERNEL);
    if (!phy)
        return -ENOMEM;

    phy->spi_dev = client;


    i2c_set_clientdata(client, phy);

	priv = pn533_register_device(PN533_DEVICE_PN532,
				     PN533_NO_TYPE_B_PROTOCOLS,
				     PN533_PROTO_REQ_ACK_RESP,
				     phy, &spi_phy_ops, NULL,
				     &phy->phy->spi_dev,
				     &client->dev);

    
    if (IS_ERR(priv)) {
        r = PTR_ERR(priv);
        return r;
    }

    phy->priv = priv;





    /* Initialize the driver data */
    spidev->spi = spi;
    spin_lock_init(&spidev->spi_lock);// initialization spin lock
    mutex_init(&spidev->buf_lock);// initialization Mutex lock
    INIT_LIST_HEAD(&spidev->device_entry);// initialization device chain table

    //init fp_data
    fp_data = kzalloc(sizeof(struct gsl_fp_data), GFP_KERNEL);
    if(fp_data == NULL){
        status = -ENOMEM;
        return status;
    }
    //set fp_data struct value
    fp_data->spidev = spidev;

    mutex_lock(&device_list_lock);//upper Mutex lock
    minor = find_first_zero_bit(minors, N_SPI_MINORS);//search the first bit whose value is 0 in the memory area
    if (minor < N_SPI_MINORS) {
        struct device *dev;
        spidev->devt = MKDEV(spidev_major, minor);
        dev = device_create(spidev_class, &spi->dev, spidev->devt, spidev, "silead_fp_dev"); create device node under /dev/
        status = IS_ERR(dev) ? PTR_ERR(dev) : 0;
    } else {
        dev_dbg(&spi->dev, "no minor number available!\n");
        status = -ENODEV;
    }
    if (status == 0) {
        set_bit(minor, minors);
        list_add(&spidev->device_entry, &device_list);//add to device chain table
    }
    mutex_unlock(&device_list_lock);//unlock Mutex lock
    if (status == 0)
        spi_set_drvdata(spi, spidev);
    else
        kfree(spidev);
    printk("%s:name=%s,bus_num=%d,cs=%d,mode=%d,speed=%d\n",__func__,spi->modalias, spi->master->bus_num, spi->chip_select, spi->mode, 
    spi->max_speed_hz);//print SPI information
    return status;
}