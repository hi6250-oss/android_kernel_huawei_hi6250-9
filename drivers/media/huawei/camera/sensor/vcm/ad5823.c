/*
 *  Hisilicon K3 SOC camera driver source file
 *
 *  Copyright (C) Huawei Technology Co., Ltd.
 *
 * Author:
 * Email:
 * Date:	  2014-11-15
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include "hwvcm.h"

//lint -save -e31

#define VCM_ID_CODE		0x5823
#define I2V(i) container_of(i, vcm_t, intf)

static hw_vcm_vtbl_t s_ad5823_vtbl;

static vcm_t s_ad5823 =
{
	.intf = { .vtbl = &s_ad5823_vtbl, },
};

int ad5823_get_otp_af(hw_vcm_intf_t *vcm_intf, void *data)
{
	/* todo */
	return 0;
}

int ad5823_i2c_read(hw_vcm_intf_t *vcm_intf, void *data)
{
	/* todo */
	return VCM_ID_CODE;
}

int ad5823_i2c_write(hw_vcm_intf_t *vcm_intf, void *data)
{
	struct hw_vcm_cfg_data *cdata = (struct hw_vcm_cfg_data *)data;
	int rc = 0;
	vcm_t* vcm = I2V(vcm_intf);

	cam_debug("%s: address=0x%x, value=0x%x\n", __func__,
		cdata->cfg.reg.address, cdata->cfg.reg.value);

	rc = hw_isp_write_vcm(vcm->vcm_info->slave_address,
			(uint16_t)cdata->cfg.reg.address,
			(uint16_t)cdata->cfg.reg.value,
			(i2c_length)vcm->vcm_info->data_type);

	return rc;
}

int ad5823_ioctl(hw_vcm_intf_t *vcm_intf, void *data)
{
	struct hw_vcm_cfg_data *cdata;
	int rc = 0;

	if (NULL == vcm_intf || NULL == data){
		cam_err("func %s: vcm_intf or data is NULL",__func__);
		return -1;
	}

    cdata = (struct hw_vcm_cfg_data*)data;

	cam_debug("%s enter.\n", __func__);

	switch (cdata->cfgtype) {
	case CFG_VCM_SET_CFG:
		break;
	case CFG_VCM_GET_OTP_AF:
		rc = ad5823_get_otp_af(vcm_intf, data);
		break;
	default:
		rc = -EFAULT;
		cam_err("%s, invalid vcm type(%d)! \n", __func__, cdata->cfgtype);
		break;
	}

	return rc;
}

static hw_vcm_vtbl_t
s_ad5823_vtbl =
{
	.vcm_config = hw_vcm_config,
	.vcm_i2c_read = ad5823_i2c_read,
	.vcm_i2c_write = ad5823_i2c_write,
	.vcm_ioctl = ad5823_ioctl,
};

static int32_t ad5823_platform_probe(struct platform_device *pdev)
{
	int rc = 0;
	cam_debug("enter %s",__func__);

	if (pdev->dev.of_node) {
		rc = hw_vcm_get_dt_data(pdev, &s_ad5823);
		if (rc < 0) {
			cam_err("%s failed line %d\n", __func__, __LINE__);
			goto ad5823_probe_fail;
		}
	} else {
		cam_err("%s ad5823 of_node is NULL.\n", __func__);
		goto ad5823_probe_fail;
	}

	rc = hw_vcm_register(pdev, &s_ad5823.intf, s_ad5823.vcm_info);
ad5823_probe_fail:
	return rc;
}

static const struct of_device_id hw_ad5823_dt_match[] = {
	{.compatible = "hisi,ad5823"},
	{}
};
MODULE_DEVICE_TABLE(of, hw_ad5823_dt_match);
static struct platform_driver hw_ad5823_platform_driver = {
	.driver = {
		.name = "ad5823",
		.owner = THIS_MODULE,
		.of_match_table = hw_ad5823_dt_match,
	},
};

static int __init hw_ad5823_module_init(void)
{
	int rc = 0;
	cam_debug("%s:%d\n", __func__, __LINE__);

	rc = platform_driver_probe(&hw_ad5823_platform_driver,
		ad5823_platform_probe);
	if (rc < 0) {
		cam_notice("%s platform_driver_probe error.\n", __func__);
	}
	return rc;
}

static void __exit hw_ad5823_module_exit(void)
{
	hw_vcm_unregister(s_ad5823.intf.subdev);
	platform_driver_unregister(&hw_ad5823_platform_driver);
}

module_init(hw_ad5823_module_init);
module_exit(hw_ad5823_module_exit);
MODULE_DESCRIPTION("AD5823 VCM");
MODULE_LICENSE("GPL v2");

//lint -restore
