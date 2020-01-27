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

static int pn533_spi_send_ack(struct pn533 *dev, gfp_t flags)
{
    struct pn533_spi_phy *phy = dev->phy;
    struct spi_device *spi_dev = phy->spi_dev;
    static const u8 ack[6] = {0x00, 0x00, 0xff, 0x00, 0xff, 0x00};
    /* spec 6.2.1.3:  Preamble, SoPC (2), ACK Code (2), Postamble */
    int rc;

    rc = spi_write(spi_dev, ack, 6);

    if (rc >= 0) {
        if (rc != 6)
            rc = -EREMOTEIO;
        else
            rc = 0;
    }

    return rc;
}


static int pn533_spi_send_frame(struct pn533 *dev,
                struct sk_buff *out)
{
    struct pn533_spi_phy *phy = dev->phy;
    struct spi_device *spi_dev = phy->spi_dev;
    int rc;

    if (phy->hard_fault != 0)
        return phy->hard_fault;

    if (phy->priv == NULL)
        phy->priv = dev;

    phy->aborted = false;

    print_hex_dump_debug("PN533_SPI: ", DUMP_PREFIX_NONE, 16, 1,
                 out->data, out->len, false);

    rc = spi_write(spi_dev, out->data, out->len);

    if (rc == -EREMOTEIO) { /* Retry, chip was in power down */
        usleep_range(6000, 10000);
        rc = spi_write(spi_dev, out->data, out->len);
    }

    if (rc >= 0) {
        if (rc != out->len)
            rc = -EREMOTEIO;
        else
            rc = 0;
    }

    return rc;
}





static int pn533_spi_read(struct pn533_i2c_phy *phy, struct sk_buff **skb)
{
    struct pn533_spi_phy *phy = phy;
    int len = PN533_EXT_FRAME_HEADER_LEN +
          PN533_STD_FRAME_MAX_PAYLOAD_LEN +
          PN533_STD_FRAME_TAIL_LEN + 1;
    int r;

    *skb = alloc_skb(len, GFP_KERNEL);
    if (*skb == NULL)
        return -ENOMEM;
    
    r = spi_read(phy->spi_dev, skb_put(*skb, len), len);
    if (r != len) {
        nfc_err(&phy->spi_dev->dev, "cannot read. r=%d len=%d\n", r, len);
        kfree_skb(*skb);
        return -EREMOTEIO;
    }

    if (!((*skb)->data[0] & 0x01)) {
        nfc_err(&phy->spi_dev->dev, "READY flag not set");
        kfree_skb(*skb);
        return -EBUSY;
    }

    /* remove READY byte */
    skb_pull(*skb, 1);
    /* trim to frame size */
    skb_trim(*skb, phy->priv->ops->rx_frame_size((*skb)->data));

    return 0;
}


static void pn533_spi_abort_cmd(struct pn533 *dev, gfp_t flags)
{
    struct pn533_spi_phy *phy = dev->phy;

    phy->aborted = true;

    /* An ack will cancel the last issued command */
    pn533_spi_send_ack(dev, flags);

    /* schedule cmd_complete_work to finish current command execution */
    pn533_recv_frame(phy->priv, NULL, -ENOENT);
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



}
