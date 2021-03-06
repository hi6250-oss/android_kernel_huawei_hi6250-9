
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif


/*****************************************************************************
  1 头文件包含
*****************************************************************************/
#include "oal_types.h"
#include "oal_ext_if.h"
#include "oal_net.h"
#if (_PRE_MULTI_CORE_MODE_OFFLOAD_DMAC == _PRE_MULTI_CORE_MODE)
#include "oal_schedule.h"
#endif
#include "oam_ext_if.h"
#include "frw_ext_if.h"
#include "hal_ext_if.h"
#include "mac_regdomain.h"
#include "mac_resource.h"
#include "mac_device.h"
#include "mac_ie.h"
#include "dmac_scan.h"
#include "dmac_main.h"
#include "dmac_acs.h"
#include "dmac_tx_bss_comm.h"
#include "dmac_ext_if.h"
#include "dmac_device.h"
#include "dmac_mgmt_sta.h"
#include "dmac_alg.h"
#if defined(_PRE_WLAN_CHIP_TEST) && (_PRE_OS_VERSION_LINUX == _PRE_OS_VERSION)
#include "dmac_scan_test.h"
#endif

#ifdef _PRE_WLAN_FEATURE_STA_PM
#include "dmac_sta_pm.h"
#include "pm_extern.h"
#endif
#if defined(_PRE_PRODUCT_ID_HI110X_DEV)
#include "dmac_dft.h"
#endif
#include "hal_device.h"
#include "dmac_config.h"
#include "dmac_chan_mgmt.h"
#include "dmac_mgmt_classifier.h"
#ifdef _PRE_WLAN_FEATURE_11K
#include "dmac_11k.h"
#endif
#undef  THIS_FILE_ID
#define THIS_FILE_ID OAM_FILE_ID_DMAC_SCAN_C


/*****************************************************************************
  2 全局变量定义
*****************************************************************************/
/* 静态函数声明 */

OAL_STATIC oal_uint32 dmac_scan_send_bcast_probe(mac_device_stru *pst_mac_device, oal_uint8 uc_band, oal_uint8  uc_index);

#if 0
#ifdef _PRE_WLAN_FEATURE_DBAC
OAL_STATIC oal_uint32  dmac_scan_register_scan_req_to_dbac(mac_device_stru *pst_mac_device);
#endif
#endif
OAL_STATIC oal_uint32  dmac_scan_report_channel_statistics_result(mac_device_stru *pst_mac_device, oal_uint8 uc_scan_idx);

OAL_STATIC oal_uint32  dmac_scan_switch_home_channel_work_timeout(void *p_arg);

OAL_STATIC oal_uint32  dmac_scan_start_pno_sched_scan_timer(void *p_arg);


/*****************************************************************************
  3 函数实现
*****************************************************************************/
#if 0
OAL_STATIC oal_void dmac_scan_print_time_stamp()
{
    oal_uint32                  ul_timestamp;

    ul_timestamp = (oal_uint32)OAL_TIME_GET_STAMP_MS();

    OAM_ERROR_LOG1(0, OAM_SF_SCAN, "{dmac_scan_print_time_stamp:: time_stamp:%d.}", ul_timestamp);

    return;
}
#endif

#if (_PRE_MULTI_CORE_MODE_OFFLOAD_DMAC == _PRE_MULTI_CORE_MODE)
#ifdef _PRE_WLAN_FEATURE_20_40_80_COEXIST

oal_void dmac_detect_2040_te_a_b(dmac_vap_stru *pst_dmac_vap, oal_uint8 *puc_frame_body, oal_uint16 us_frame_len, oal_uint16 us_offset,oal_uint8 uc_curr_chan)
{
    oal_uint8            chan_index     = 0;
    oal_bool_enum_uint8  ht_cap         = OAL_FALSE;
    oal_uint8            uc_real_chan   = uc_curr_chan;
    mac_device_stru     *pst_mac_device = OAL_PTR_NULL;
    oal_uint8           *puc_ie         = OAL_PTR_NULL;

    pst_mac_device = mac_res_get_dev(pst_dmac_vap->st_vap_base_info.uc_device_id);
    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_mac_device))
    {
        OAM_WARNING_LOG0(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN,
                         "{dmac_detect_2040_te_a_b::mac_res_get_dev return null.}");
        return;
    }

    if (us_frame_len <= us_offset)
    {
        mac_get_channel_idx_from_num((pst_mac_device->st_scan_params.ast_channel_list[pst_mac_device->uc_scan_chan_idx]).en_band,
                                      uc_real_chan, &chan_index);
        /* Detect Trigger Event - A */
        pst_dmac_vap->st_vap_base_info.st_ch_switch_info.ul_chan_report_for_te_a |= (1 << chan_index);

        OAM_WARNING_LOG1(0, OAM_SF_ANY, "{dmac_detect_2040_te_a_b::framebody_len[%d]}", us_frame_len);
        return;
    }

    us_frame_len   -= us_offset;
    puc_frame_body += us_offset;

    puc_ie = mac_find_ie(MAC_EID_HT_CAP, puc_frame_body, us_frame_len);
    if (OAL_PTR_NULL != puc_ie)
    {
        ht_cap = OAL_TRUE;

        /* Check for the Forty MHz Intolerant bit in HT-Capabilities */
        if((puc_ie[3] & BIT6) != 0)
        {
            //OAM_INFO_LOG0(0, OAM_SF_SCAN, "dmac_detect_2040_te_a_b::40 intolerant in ht cap");
            /* Register Trigger Event - B */
            pst_dmac_vap->st_vap_base_info.st_ch_switch_info.en_te_b = OAL_TRUE;
        }
    }

    puc_ie = mac_find_ie(MAC_EID_2040_COEXT, puc_frame_body, us_frame_len);
    if (OAL_PTR_NULL != puc_ie)
    {
        /* Check for the Forty MHz Intolerant bit in Coex-Mgmt IE */
        if((puc_ie[2] & BIT1) != 0)
        {
            //OAM_INFO_LOG0(0, OAM_SF_SCAN, "dmac_detect_2040_te_a_b::40 intolerant in co");
            /* Register Trigger Event - B */
            pst_dmac_vap->st_vap_base_info.st_ch_switch_info.en_te_b = OAL_TRUE;
        }

    }

    /* 只有在HT能力为False时才需要获取信道信息，其他情况下不需要查找 */
    if(OAL_FALSE == ht_cap)
    {
        puc_ie = mac_find_ie(MAC_EID_DSPARMS, puc_frame_body, us_frame_len);
        if (OAL_PTR_NULL != puc_ie)
        {
            uc_real_chan = puc_ie[2];
        }

        mac_get_channel_idx_from_num((pst_mac_device->st_scan_params.ast_channel_list[pst_mac_device->uc_scan_chan_idx]).en_band,
                                      uc_real_chan, &chan_index);
        pst_dmac_vap->st_vap_base_info.st_ch_switch_info.ul_chan_report_for_te_a |= (1 << chan_index);
    }

    return;
}
#endif

oal_void  dmac_scan_proc_obss_scan_complete_event(dmac_vap_stru *pst_dmac_vap)
{
#if 0
    OAM_INFO_LOG2(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN,
                 "{dmac_scan_proc_obss_scan_complete_event::te_a:%d,te_b:%d}",
                 pst_dmac_vap->st_vap_base_info.st_ch_switch_info.ul_chan_report_for_te_a,
                 pst_dmac_vap->st_vap_base_info.st_ch_switch_info.en_te_b);
#endif

    if(!pst_dmac_vap->st_vap_base_info.st_ch_switch_info.ul_chan_report_for_te_a
       && (OAL_FALSE == pst_dmac_vap->st_vap_base_info.st_ch_switch_info.en_te_b))
    {
        return;
    }

    dmac_send_2040_coext_mgmt_frame_sta(&(pst_dmac_vap->st_vap_base_info));

    return;
}
#endif


OAL_STATIC oal_void  dmac_scan_set_vap_mac_addr_by_scan_state(mac_device_stru  *pst_mac_device,
                                                                           oal_bool_enum_uint8 en_is_scan_start)
{
    dmac_vap_stru               *pst_dmac_vap;
    mac_vap_stru                *pst_mac_vap;
    mac_scan_req_stru           *pst_scan_params;

    /* 获取扫描参数 */
    pst_scan_params = &(pst_mac_device->st_scan_params);

    /* 非随机mac addr扫描，直接返回，无需重设帧过滤寄存器 */
    if (OAL_TRUE != pst_scan_params->en_is_random_mac_addr_scan)
    {
        //OAM_INFO_LOG0(0, OAM_SF_SCAN, "{dmac_scan_set_vap_mac_addr_by_scan_state:: don't need modified mac addr.}");
        return;
    }

    /* p2p扫描不支持随机mac addr */
    if (OAL_TRUE == pst_scan_params->bit_is_p2p0_scan)
    {
        //OAM_INFO_LOG0(0, OAM_SF_SCAN, "{dmac_scan_set_vap_mac_addr_by_scan_state:: p2p scan, don't need modified mac addr.}");
        return;
    }

    /* 获取dmac vap */
    pst_dmac_vap = mac_res_get_dmac_vap(pst_scan_params->uc_vap_id);
    if (OAL_PTR_NULL == pst_dmac_vap)
    {
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "{dmac_scan_set_vap_mac_addr_by_scan_state:: pst_dmac_vap is null.}");
        return;
    }

    pst_mac_vap = &(pst_dmac_vap->st_vap_base_info);

    /* 判断当前非P2P场景，进行随机MAC ADDR的设置 */
    if (!IS_LEGACY_VAP(pst_mac_vap))
    {
        return;
    }

    /* 扫描开始时，重新设置帧过滤寄存器 */
    if (OAL_TRUE == en_is_scan_start)
    {
        /* 保存原先的mac addr */
        oal_set_mac_addr(pst_mac_device->auc_original_mac_addr, pst_mac_vap->pst_mib_info->st_wlan_mib_sta_config.auc_dot11StationID);

        /* 设置mib和硬件的macaddr为随机mac addr */
        oal_set_mac_addr(pst_mac_vap->pst_mib_info->st_wlan_mib_sta_config.auc_dot11StationID,
                         pst_scan_params->auc_sour_mac_addr);
        hal_vap_set_macaddr(pst_dmac_vap->pst_hal_vap, pst_scan_params->auc_sour_mac_addr);
#if (defined(_PRE_PRODUCT_ID_HI110X_DEV))
        OAM_WARNING_LOG_ALTER(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN, "{dmac_scan_set_vap_mac_addr_by_scan_state::original mac addr[%02X:XX:XX:%02X:%02X:%02X], set random mac addr[%02X:XX:XX:%02X:%02X:%02X]}", 8,
                                 pst_mac_device->auc_original_mac_addr[0], pst_mac_device->auc_original_mac_addr[3],
                                 pst_mac_device->auc_original_mac_addr[4], pst_mac_device->auc_original_mac_addr[5],
                                 pst_scan_params->auc_sour_mac_addr[0], pst_scan_params->auc_sour_mac_addr[3],
                                 pst_scan_params->auc_sour_mac_addr[4], pst_scan_params->auc_sour_mac_addr[5]);
#endif
    }
    else
    {
        /* 扫描结束，恢复原先mib和硬件寄存器的mac addr */
        oal_set_mac_addr(pst_mac_vap->pst_mib_info->st_wlan_mib_sta_config.auc_dot11StationID,
                         pst_mac_device->auc_original_mac_addr);
        hal_vap_set_macaddr(pst_dmac_vap->pst_hal_vap, pst_mac_device->auc_original_mac_addr);

        OAM_WARNING_LOG4(0, OAM_SF_SCAN, "{dmac_scan_set_vap_mac_addr_by_scan_state:: resume original mac addr, mac_addr:%02X:XX:XX:%02X:%02X:%02X.}",
                         pst_mac_device->auc_original_mac_addr[0], pst_mac_device->auc_original_mac_addr[3], pst_mac_device->auc_original_mac_addr[4], pst_mac_device->auc_original_mac_addr[5]);
    }

    return;
}


OAL_STATIC oal_uint32  dmac_scan_report_scanned_bss(dmac_vap_stru *pst_dmac_vap, oal_void *p_param)
{
    frw_event_mem_stru                    *pst_event_mem;
    frw_event_stru                        *pst_event;
    mac_device_stru                       *pst_mac_device;
    dmac_tx_event_stru                    *pst_dtx_event;
    oal_netbuf_stru                       *pst_netbuf;
    dmac_rx_ctl_stru                      *pst_rx_ctrl;
    oal_uint8                             *puc_frame_body_tail;            /* 指向帧体的尾部 */
    mac_scan_req_stru                     *pst_scan_params;
    mac_scanned_result_extend_info_stru   *pst_scanned_result_extend_info;
    oal_uint16                             us_frame_len;
    oal_uint16                             us_remain_netbuf_len;

    pst_mac_device = mac_res_get_dev(pst_dmac_vap->st_vap_base_info.uc_device_id);
    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_mac_device))
    {
        OAM_WARNING_LOG0(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN,
                         "{dmac_scan_report_scanned_bss::pst_mac_device null.}");
        return OAL_ERR_CODE_PTR_NULL;
    }

    pst_netbuf = (oal_netbuf_stru *)p_param;

    /* 获取帧信息 */
    us_frame_len = (oal_uint16)oal_netbuf_get_len(pst_netbuf);

#if (_PRE_MULTI_CORE_MODE_OFFLOAD_DMAC == _PRE_MULTI_CORE_MODE)
    us_remain_netbuf_len = oal_netbuf_get_payload_len(pst_netbuf) - (us_frame_len - MAC_80211_FRAME_LEN);
#else
    us_remain_netbuf_len = (oal_uint16)oal_netbuf_tailroom(pst_netbuf);
#endif

    if(us_remain_netbuf_len < OAL_SIZEOF(mac_scanned_result_extend_info_stru))
    {
        /*to cut one netbuf mem.*/
        /*tailroom not enough*/
        OAM_ERROR_LOG2(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN,
                         "{dmac_scan_report_scanned_bss::scan netbuf tailroom not enough,requet[%u],actual[%u] }",
                         us_remain_netbuf_len,
                         OAL_SIZEOF(mac_scanned_result_extend_info_stru));
        return OAL_FAIL;
    }

    /* 获取该buffer的控制信息 */
    pst_rx_ctrl = (dmac_rx_ctl_stru *)oal_netbuf_cb(pst_netbuf);

    /* 获取扫描参数 */
    pst_scan_params = &(pst_mac_device->st_scan_params);

    /* 每次扫描结束，上报扫描结果，抛事件到HMAC, 申请事件内存 */
    pst_event_mem = FRW_EVENT_ALLOC(OAL_SIZEOF(dmac_tx_event_stru));
    if (OAL_PTR_NULL == pst_event_mem)
    {
        OAM_ERROR_LOG0(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN,
                       "{dmac_scan_report_scanned_bss::pst_event_mem null.}");
        return OAL_ERR_CODE_PTR_NULL;
    }

    /* 填写事件 */
    pst_event = (frw_event_stru *)pst_event_mem->puc_data;

    FRW_EVENT_HDR_INIT(&(pst_event->st_event_hdr),
                       FRW_EVENT_TYPE_WLAN_CRX,
                       DMAC_WLAN_CRX_EVENT_SUB_TYPE_EVERY_SCAN_RESULT,
                       OAL_SIZEOF(dmac_tx_event_stru),
                       FRW_EVENT_PIPELINE_STAGE_1,
                       pst_dmac_vap->st_vap_base_info.uc_chip_id,
                       pst_dmac_vap->st_vap_base_info.uc_device_id,
                       pst_dmac_vap->st_vap_base_info.uc_vap_id);

    /***********************************************************************************************/
    /*            netbuf data域的上报的扫描结果的字段的分布                                        */
    /* ------------------------------------------------------------------------------------------  */
    /* beacon/probe rsp body  |     帧体后面附加字段(mac_scanned_result_extend_info_stru)          */
    /* -----------------------------------------------------------------------------------------   */
    /* 收到的beacon/rsp的body | rssi(4字节) | channel num(1字节)| band(1字节)|bss_tye(1字节)|填充  */
    /* ------------------------------------------------------------------------------------------  */
    /*                                                                                             */
    /***********************************************************************************************/

    /* 将扩展信息拷贝到帧体的后面，上报到host侧 */
#if (_PRE_MULTI_CORE_MODE_OFFLOAD_DMAC == _PRE_MULTI_CORE_MODE)
    puc_frame_body_tail = (oal_uint8 *)MAC_GET_RX_PAYLOAD_ADDR(&(pst_rx_ctrl->st_rx_info), pst_netbuf) + us_frame_len - MAC_80211_FRAME_LEN;
#else
    puc_frame_body_tail = (oal_uint8 *)mac_get_rx_cb_mac_hdr(&(pst_rx_ctrl->st_rx_info)) + us_frame_len;
#endif

    /* 指向帧体的尾部，对netbuf进行扩展，携带其它需要上报的信息 */
    pst_scanned_result_extend_info = (mac_scanned_result_extend_info_stru *)puc_frame_body_tail;

    /* 清空上报扫描结果的扩展字段信息并进行赋值 */
    OAL_MEMZERO(pst_scanned_result_extend_info, OAL_SIZEOF(mac_scanned_result_extend_info_stru));
    pst_scanned_result_extend_info->l_rssi = (oal_int32)pst_rx_ctrl->st_rx_statistic.c_rssi_dbm;
    pst_scanned_result_extend_info->en_bss_type = pst_scan_params->en_bss_type;

    /* 修正帧的长度为加上扩展字段的长度 */
    us_frame_len += OAL_SIZEOF(mac_scanned_result_extend_info_stru);

    /* 业务事件信息 */
    pst_dtx_event               = (dmac_tx_event_stru *)pst_event->auc_event_data;
    pst_dtx_event->pst_netbuf   = pst_netbuf;
    pst_dtx_event->us_frame_len = us_frame_len;

#if 0
    /* 加入维测信息，当前信道号、信号强度、上报的netbuf长度 */
    OAM_ERROR_LOG4(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN,
                   "{dmac_scan_report_scanned_bss::rssi[%d], cb_rssi[%d], channel_num[%d], buf_len[%d].}",
                   pst_scanned_result_extend_info->l_rssi,
                   pst_rx_ctrl->st_rx_statistic.c_rssi_dbm,
                   pst_rx_ctrl->st_rx_info.uc_channel_number,
                   us_frame_len);
#endif

    /* 分发事件 */
    frw_event_dispatch_event(pst_event_mem);
    FRW_EVENT_FREE(pst_event_mem);

    return OAL_SUCC;
}


OAL_STATIC oal_uint32  dmac_scan_check_bss_in_pno_scan(oal_uint8         *puc_frame_body,
                                                                  oal_int32          l_frame_body_len,
                                                                  mac_pno_scan_stru *pst_pno_scan_info,
                                                                  oal_int32          l_rssi)
{
    oal_uint8       *puc_ssid;
    oal_int32        l_loop;
    oal_int8         ac_ssid[WLAN_SSID_MAX_LEN];
    oal_uint8        uc_ssid_len = 0;

    /* 如果该帧的信号小于pno扫描可上报的阈值，则返回失败，此帧不上报 */
    if (l_rssi < pst_pno_scan_info->l_rssi_thold)
    {
        return OAL_FAIL;
    }

    OAL_MEMZERO(ac_ssid, OAL_SIZEOF(ac_ssid));

    /* 获取管理帧中的ssid IE信息 */
    puc_ssid = mac_get_ssid(puc_frame_body, l_frame_body_len, &uc_ssid_len);
    if ((OAL_PTR_NULL != puc_ssid) && (0 != uc_ssid_len))
    {
        oal_memcopy(ac_ssid, puc_ssid, uc_ssid_len);
        ac_ssid[uc_ssid_len] = '\0';
    }

    /* 从pno参数中查找本ssid是否存在，如果存在，则可以上报 */
    for (l_loop = 0; l_loop < pst_pno_scan_info->l_ssid_count; l_loop++)
    {
        /* 如果ssid相同，返回成功 */
        if (0 == oal_memcmp(ac_ssid, pst_pno_scan_info->ast_match_ssid_set[l_loop].auc_ssid, (uc_ssid_len + 1)))
        {
            OAM_WARNING_LOG0(0, OAM_SF_SCAN, "{dmac_scan_check_bss_in_pno_scan::ssid match success.}");
            return OAL_SUCC;
        }
    }

    return OAL_FAIL;
}


OAL_STATIC oal_uint32  dmac_scan_check_bss_type(oal_uint8 *puc_frame_body, mac_scan_req_stru *pst_scan_params)
{
    mac_cap_info_stru         *pst_cap_info;

    /*************************************************************************/
    /*                       Beacon Frame - Frame Body                       */
    /* ----------------------------------------------------------------------*/
    /* |Timestamp|BcnInt|CapInfo|SSID|SupRates|DSParamSet|TIM  |CountryElem |*/
    /* ----------------------------------------------------------------------*/
    /* |8        |2     |2      |2-34|3-10    |3         |6-256|8-256       |*/
    /* ----------------------------------------------------------------------*/
    pst_cap_info = (mac_cap_info_stru *)(puc_frame_body + MAC_TIME_STAMP_LEN + MAC_BEACON_INTERVAL_LEN);

    if ((WLAN_MIB_DESIRED_BSSTYPE_INFRA == pst_scan_params->en_bss_type) &&
        (1 != pst_cap_info->bit_ess))
    {
        //OAM_INFO_LOG0(0, OAM_SF_SCAN, "{dmac_scan_check_bss_type::expect infra bss, but it's not infra bss.}\r\n");
        return OAL_FAIL;
    }

    if ((WLAN_MIB_DESIRED_BSSTYPE_INDEPENDENT == pst_scan_params->en_bss_type) &&
        (1 != pst_cap_info->bit_ibss))
    {
        //OAM_INFO_LOG0(0, OAM_SF_SCAN, "{dmac_scan_check_bss_type::expect ibss, but it's not ibss.}\r\n");
        return OAL_FAIL;
    }

    return OAL_SUCC;
}

#define DMAC_CSA_RSP_TIMEOUT 120000
frw_timeout_stru g_csa_stop_timer = {0};
#ifdef _PRE_WLAN_FEATURE_20_40_80_COEXIST

oal_void  dmac_scan_check_assoc_ap_channel(dmac_vap_stru *pst_dmac_vap, mac_device_stru *pst_mac_device, oal_netbuf_stru *pst_netbuf)
{
    dmac_rx_ctl_stru                        *pst_rx_ctrl;
    mac_ieee80211_frame_stru                *pst_frame_hdr;
    oal_uint8                               *puc_frame_body;
    oal_uint16                              us_frame_len;
    oal_uint16                              us_offset =  MAC_TIME_STAMP_LEN + MAC_BEACON_INTERVAL_LEN + MAC_CAP_INFO_LEN;
    mac_cfg_ssid_param_stru                 st_mib_ssid = {0};
    oal_uint8                               uc_mib_ssid_len = 0;
    oal_uint8                               uc_frame_channel;
    oal_uint8                               uc_ssid_len = 0;
    oal_uint8                               *puc_ssid;
    oal_uint32                              ul_ret;
    oal_uint8                               uc_idx;
    mac_scan_req_stru                       *pst_scan_params = &(pst_mac_device->st_scan_params);

    pst_rx_ctrl = (dmac_rx_ctl_stru *)oal_netbuf_cb(pst_netbuf);

    /* 获取帧信息 */
    pst_frame_hdr  = (mac_ieee80211_frame_stru *)mac_get_rx_cb_mac_hdr(&(pst_rx_ctrl->st_rx_info));
    puc_frame_body = MAC_GET_RX_PAYLOAD_ADDR(&(pst_rx_ctrl->st_rx_info), pst_netbuf);
    us_frame_len   = (oal_uint16)oal_netbuf_get_len(pst_netbuf);

    if (!OAL_MEMCMP(pst_frame_hdr->auc_address3, pst_dmac_vap->st_vap_base_info.auc_bssid, WLAN_MAC_ADDR_LEN))
    {
        puc_ssid = mac_get_ssid(puc_frame_body, (oal_int32)(us_frame_len - MAC_80211_FRAME_LEN), &uc_ssid_len);
        mac_mib_get_ssid(&pst_dmac_vap->st_vap_base_info, &uc_mib_ssid_len, (oal_uint8 *)&st_mib_ssid);
        if ((OAL_PTR_NULL != puc_ssid) && (0 != uc_ssid_len) && (st_mib_ssid.uc_ssid_len == uc_ssid_len))
        {
            if (!OAL_MEMCMP(st_mib_ssid.ac_ssid, puc_ssid, uc_ssid_len))
            {
                /* 获取管理帧中的信道 */
                uc_frame_channel = mac_ie_get_chan_num(puc_frame_body, (us_frame_len - MAC_80211_FRAME_LEN),
                                   us_offset, pst_rx_ctrl->st_rx_info.uc_channel_number);

                if (((pst_dmac_vap->st_vap_base_info.st_channel.uc_chan_number != uc_frame_channel &&
                    pst_scan_params->en_scan_mode != WLAN_SCAN_MODE_BACKGROUND_CSA) ||
                    (g_st_old_channel.uc_chan_number == pst_rx_ctrl->st_rx_info.uc_channel_number &&
                    pst_scan_params->en_scan_mode == WLAN_SCAN_MODE_BACKGROUND_CSA)) && (0 != uc_frame_channel))
                {
                    if (pst_scan_params->en_scan_mode == WLAN_SCAN_MODE_BACKGROUND_CSA) {
                        // 原信道收到AP响应帧认为是攻击场景，两分钟内不做CSA响应
                        pst_dmac_vap->st_vap_base_info.st_ch_switch_info.uc_switch_fail = OAL_TRUE;
                        if (!g_csa_stop_timer.en_is_registerd) {
                            FRW_TIMER_CREATE_TIMER(&g_csa_stop_timer, dmac_sta_csa_stop_timeout_fn,
                                DMAC_CSA_RSP_TIMEOUT, (void *)pst_dmac_vap, OAL_FALSE,
                                OAM_MODULE_ID_DMAC, pst_dmac_vap->st_vap_base_info.ul_core_id);
                        }
                        uc_frame_channel = pst_rx_ctrl->st_rx_info.uc_channel_number;
                    }
                    /* AP热点不可能变更频段,只关注信道 */
                    ul_ret = mac_get_channel_idx_from_num(pst_dmac_vap->st_vap_base_info.st_channel.en_band, uc_frame_channel, &uc_idx);
                    if (OAL_SUCC != ul_ret)
                    {
                        OAM_ERROR_LOG2(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_ANY, "{dmac_scan_check_assoc_ap_channel::channel[%d],mac_get_channel_idx_from_num failed[%d].}", uc_frame_channel,ul_ret);
                        return;
                    }
                    /* 信道切换,使用默认带宽 */
                    pst_dmac_vap->st_vap_base_info.st_ch_switch_info.uc_new_channel    = uc_frame_channel;
                    pst_dmac_vap->st_vap_base_info.st_ch_switch_info.en_new_bandwidth = pst_dmac_vap->st_vap_base_info.st_channel.en_bandwidth;

                    /* 如下2个变量可以唯一确认是本次扫描引发的信道切换,因为CSA IE导致的切换uc_new_ch_swt_cnt是大于0的 */
                    pst_dmac_vap->st_vap_base_info.st_ch_switch_info.en_waiting_to_shift_channel = OAL_TRUE;
                    pst_dmac_vap->st_vap_base_info.st_ch_switch_info.uc_new_ch_swt_cnt = 0;
                    OAM_WARNING_LOG1(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_ANY, "{dmac_scan_check_assoc_ap_channel::AP is changed to new channel[%d].}", uc_frame_channel);

                }
            }
        }
    }
}
#endif


oal_uint32  dmac_scan_mgmt_filter(dmac_vap_stru *pst_dmac_vap, oal_void *p_param, oal_bool_enum_uint8 *pen_report_bss, oal_uint8 *pen_go_on)
{
    /* !!! 注意:dmac_rx_filter_mgmt 会根据pen_report_bss 决定是否需要释放netbuf,根据pen_go_on标志是否需要继续上报 */
    /* 如果pen_report_bss返回为OAL_TRUE代表扫描接口已经上报,dmac_rx_filter_mgmt则不会释放netbuf,否则根据pen_go_on标志继续上报或释放netbuf */

    oal_netbuf_stru            *pst_netbuf;
    dmac_rx_ctl_stru           *pst_rx_ctrl;
    mac_ieee80211_frame_stru   *pst_frame_hdr;
    oal_uint8                  *puc_frame_body;
    mac_device_stru            *pst_mac_device;
    mac_scan_req_stru          *pst_scan_params;
    oal_uint32                  ul_ret;
    oal_uint16                  us_frame_len;

#if (_PRE_MULTI_CORE_MODE_OFFLOAD_DMAC == _PRE_MULTI_CORE_MODE)
#ifdef _PRE_WLAN_FEATURE_20_40_80_COEXIST
    oal_uint16                  us_offset =  MAC_TIME_STAMP_LEN + MAC_BEACON_INTERVAL_LEN + MAC_CAP_INFO_LEN;
#endif
#endif

    pst_mac_device  = mac_res_get_dev(pst_dmac_vap->st_vap_base_info.uc_device_id);
    if (OAL_PTR_NULL == pst_mac_device)
    {
        OAM_WARNING_LOG0(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN, "{dmac_scan_mgmt_filter::pst_mac_device null.}");
        return OAL_ERR_CODE_PTR_NULL;
    }

    pst_scan_params = &(pst_mac_device->st_scan_params);

    pst_netbuf = (oal_netbuf_stru *)p_param;

    /* 获取该buffer的控制信息 */
    pst_rx_ctrl = (dmac_rx_ctl_stru *)oal_netbuf_cb(pst_netbuf);

    /* 获取帧信息 */
    pst_frame_hdr  = (mac_ieee80211_frame_stru *)mac_get_rx_cb_mac_hdr(&(pst_rx_ctrl->st_rx_info));
    puc_frame_body = MAC_GET_RX_PAYLOAD_ADDR(&(pst_rx_ctrl->st_rx_info), pst_netbuf);

    us_frame_len   = (oal_uint16)oal_netbuf_get_len(pst_netbuf);

    if ((WLAN_BEACON == pst_frame_hdr->st_frame_control.bit_sub_type) ||
        (WLAN_PROBE_RSP == pst_frame_hdr->st_frame_control.bit_sub_type))
    {
        /* 检测扫描到bss的类型 */
        if (OAL_SUCC != dmac_scan_check_bss_type(puc_frame_body, pst_scan_params))
        {
            //OAM_INFO_LOG0(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN, "{dmac_scan_mgmt_filter::scanned bss isn't the desired one.}\r\n");
            return OAL_SUCC;
        }

        /* 如果是obss扫描，不上报扫描结果，只在从信道检测到了beacon帧或者probe rsp帧，拋事件到host，让20/40共存逻辑处理 */
        if (WLAN_SCAN_MODE_BACKGROUND_OBSS == pst_scan_params->en_scan_mode)
        {
#if (_PRE_MULTI_CORE_MODE_OFFLOAD_DMAC == _PRE_MULTI_CORE_MODE)
#ifdef _PRE_WLAN_FEATURE_20_40_80_COEXIST
            dmac_detect_2040_te_a_b(pst_dmac_vap, puc_frame_body, us_frame_len, us_offset, pst_rx_ctrl->st_rx_info.uc_channel_number);
#endif
#endif
            /* OBSS扫描不需要继续上报报文内容或扫描结果到host */
            *pen_go_on = OAL_FALSE;
            return OAL_SUCC;
        }
        else
        {
            /* 处于扫描状态，且接收到的bss 信道信息与当前扫描信道相同，才标识当前信道扫描到BSS */
            if ((pst_mac_device->st_scan_params.en_working_in_home_chan == OAL_FALSE)
                && (pst_mac_device->pst_device_stru->uc_current_chan_number
                        == mac_ie_get_chan_num(puc_frame_body, (us_frame_len - MAC_80211_FRAME_LEN),
                                                                MAC_TIME_STAMP_LEN + MAC_BEACON_INTERVAL_LEN + MAC_CAP_INFO_LEN,
                                                                pst_rx_ctrl->st_rx_info.uc_channel_number)))
            {
                pst_mac_device->en_scan_curr_chan_find_bss_flag = OAL_TRUE;
            }
#ifdef _PRE_WLAN_FEATURE_20_40_80_COEXIST
            if ((WLAN_VAP_MODE_BSS_STA == pst_dmac_vap->st_vap_base_info.en_vap_mode)
                 && (MAC_VAP_STATE_PAUSE == pst_dmac_vap->st_vap_base_info.en_vap_state))
            {
                /* STA扫描且linkloss超过门限1/4时，进行AP信道是否切换的识别 */
                if (pst_dmac_vap->st_linkloss_info.us_link_loss >= pst_dmac_vap->st_linkloss_info.us_linkloss_threshold / 4)
                {
                    dmac_scan_check_assoc_ap_channel(pst_dmac_vap, pst_mac_device, pst_netbuf);
                }

                if (WLAN_SCAN_MODE_BACKGROUND_CSA == pst_scan_params->en_scan_mode)
                {
                    dmac_scan_check_assoc_ap_channel(pst_dmac_vap, pst_mac_device, pst_netbuf);
                    /* linkloss CSA扫描不需要上报报文或扫描结果到host */
                    *pen_go_on = OAL_FALSE;
                    return OAL_SUCC;
                }
            }
#endif
            /* 如果是pno调度扫描，则需要进行rssi和ssid的过滤 */
            if (WLAN_SCAN_MODE_BACKGROUND_PNO == pst_scan_params->en_scan_mode)
            {
                /* 检测本bss是否可以上报，在pno扫描的场景下 */
                ul_ret = dmac_scan_check_bss_in_pno_scan(puc_frame_body,
                                                         (oal_int32)(us_frame_len - MAC_80211_FRAME_LEN),
                                                         &(pst_mac_device->pst_pno_sched_scan_mgmt->st_pno_sched_scan_params),
                                                         (oal_int32)pst_rx_ctrl->st_rx_statistic.c_rssi_dbm);
                if (OAL_SUCC != ul_ret)
                {
                    //OAM_INFO_LOG0(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN, "{dmac_scan_mgmt_filter::this bss info can't report host.}");

                    /* PNO扫描,没有扫描到需要关联的AP,不需要上报报文或扫描结果到host */
                    *pen_go_on = OAL_FALSE;

                    return OAL_SUCC;
                }

                /* 如果是扫描到了第一个匹配的ssid，置扫描到了匹配的ssid标记位为真 */
                if (OAL_TRUE != pst_mac_device->pst_pno_sched_scan_mgmt->en_is_found_match_ssid)
                {
                    pst_mac_device->pst_pno_sched_scan_mgmt->en_is_found_match_ssid = OAL_TRUE;

                    /* 停止pno调度扫描定时器 */
                    dmac_scan_stop_pno_sched_scan_timer(pst_mac_device->pst_pno_sched_scan_mgmt);
                }
            }
#ifdef _PRE_WLAN_FEATURE_11K
            else if (WLAN_SCAN_MODE_RRM_BEACON_REQ == pst_scan_params->en_scan_mode)
            {
                /* WLAN_SCAN_MODE_RRM_BEACON_REQ扫描,不需要上报报文或扫描结果到host */
                *pen_go_on = OAL_FALSE;

                dmac_rrm_get_bcn_info_from_rx(pst_dmac_vap, pst_netbuf);
                return OAL_SUCC;
            }
#endif

            /* 其它模式扫描，上报扫描到的扫描结果 */
            ul_ret = dmac_scan_report_scanned_bss(pst_dmac_vap, p_param);
            if (OAL_SUCC != ul_ret)
            {
                *pen_report_bss = OAL_FALSE;
                OAM_ERROR_LOG0(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN, "{dmac_scan_mgmt_filter::report scan result failed.}");
                return OAL_SUCC;
            }
            else
            {
                *pen_report_bss = OAL_TRUE;
            }
        }
    }

    return OAL_SUCC;
}


OAL_STATIC oal_uint16  dmac_scan_encap_probe_req_frame(dmac_vap_stru *pst_dmac_vap, oal_netbuf_stru *pst_mgmt_buf, oal_uint8 *puc_bssid, oal_int8 *pc_ssid)
{
    oal_uint8        uc_ie_len;
    oal_uint8       *puc_mac_header          = oal_netbuf_header(pst_mgmt_buf);
#if (_PRE_MULTI_CORE_MODE_OFFLOAD_DMAC == _PRE_MULTI_CORE_MODE)
    oal_uint8       *puc_payload_addr        = oal_netbuf_data(pst_mgmt_buf);
#else
    oal_uint8       *puc_payload_addr        = puc_mac_header + MAC_80211_FRAME_LEN;
#endif
    oal_uint8       *puc_payload_addr_origin = puc_payload_addr;
    mac_device_stru *pst_mac_device = OAL_PTR_NULL;
    oal_uint16       us_app_ie_len;

    pst_mac_device = mac_res_get_dev(pst_dmac_vap->st_vap_base_info.uc_device_id);
    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_mac_device))
    {
        OAM_ERROR_LOG1(0, OAM_SF_ANY, "{dmac_scan_encap_probe_req_frame::pst_mac_device[%d] null!}", pst_dmac_vap->st_vap_base_info.uc_device_id);
        return 0;
    }
    /*************************************************************************/
    /*                        Management Frame Format                        */
    /* --------------------------------------------------------------------  */
    /* |Frame Control|Duration|DA|SA|BSSID|Sequence Control|Frame Body|FCS|  */
    /* --------------------------------------------------------------------  */
    /* | 2           |2       |6 |6 |6    |2               |0 - 2312  |4  |  */
    /* --------------------------------------------------------------------  */
    /*                                                                       */
    /*************************************************************************/

    /*************************************************************************/
    /*                Set the fields in the frame header                     */
    /*************************************************************************/
    /* 帧控制字段全为0，除了type和subtype */
    mac_hdr_set_frame_control(puc_mac_header, WLAN_PROTOCOL_VERSION| WLAN_FC0_TYPE_MGT | WLAN_FC0_SUBTYPE_PROBE_REQ);

    /* 设置分片序号为0 */
    mac_hdr_set_fragment_number(puc_mac_header, 0);

    /* 设置地址1，广播地址 */
    oal_set_mac_addr(puc_mac_header + WLAN_HDR_ADDR1_OFFSET, BROADCAST_MACADDR);

    /* 设置地址2的MAC地址(p2p扫描为p2p的地址，其它为本机地址，如果随机mac addr扫描开启，则为随机地址) */
    oal_set_mac_addr(puc_mac_header + WLAN_HDR_ADDR2_OFFSET, pst_mac_device->st_scan_params.auc_sour_mac_addr);

    /* 地址3，广播地址 */
    oal_set_mac_addr(puc_mac_header + WLAN_HDR_ADDR3_OFFSET, puc_bssid);

    /*************************************************************************/
    /*                       Probe Request Frame - Frame Body                */
    /* --------------------------------------------------------------------- */
    /* |SSID |Supported Rates |Extended supp rates| HT cap|Extended cap      */
    /* --------------------------------------------------------------------- */
    /* |2-34 |   3-10         | 2-257             |  28   | 3-8              */
    /* --------------------------------------------------------------------- */
    /*                                                                       */
    /*************************************************************************/

    /* 设置SSID */
    /***************************************************************************
                    ----------------------------
                    |Element ID | Length | SSID|
                    ----------------------------
           Octets:  |1          | 1      | 0~32|
                    ----------------------------
    ***************************************************************************/
    if ('\0' == pc_ssid[0])    /* 通配SSID */
    {
        puc_payload_addr[0] = MAC_EID_SSID;
        puc_payload_addr[1] = 0;
        puc_payload_addr   += MAC_IE_HDR_LEN;    /* 偏移buffer指向下一个ie */
    }
    else
    {
        puc_payload_addr[0] = MAC_EID_SSID;
        puc_payload_addr[1] = (oal_uint8)OAL_STRLEN(pc_ssid);
        oal_memcopy(&(puc_payload_addr[2]), pc_ssid, puc_payload_addr[1]);
        puc_payload_addr += MAC_IE_HDR_LEN + puc_payload_addr[1];  /* 偏移buffer指向下一个ie */
    }

    /* 设置支持的速率集 */
    mac_set_supported_rates_ie(&(pst_dmac_vap->st_vap_base_info), puc_payload_addr, &uc_ie_len);
    puc_payload_addr += uc_ie_len;

    /* 设置dsss参数集 */
    mac_set_dsss_params(&(pst_dmac_vap->st_vap_base_info), puc_payload_addr, &uc_ie_len);
    puc_payload_addr += uc_ie_len;

    /* 设置extended supported rates信息 */
    mac_set_exsup_rates_ie(&(pst_dmac_vap->st_vap_base_info), puc_payload_addr, &uc_ie_len);
    puc_payload_addr += uc_ie_len;

    /* PNO扫描,probe request报文只包含信道和速率集信息元素,减少发送报文长度 */
    if((MAC_SCAN_STATE_RUNNING == pst_mac_device->en_curr_scan_state)
       && (WLAN_SCAN_MODE_BACKGROUND_PNO == pst_mac_device->st_scan_params.en_scan_mode))
    {
        return (oal_uint16)(puc_payload_addr - puc_payload_addr_origin + MAC_80211_FRAME_LEN);
    }

    /* 填充HT Capabilities信息 */
    mac_set_ht_capabilities_ie(&(pst_dmac_vap->st_vap_base_info), puc_payload_addr, &uc_ie_len);
    puc_payload_addr += uc_ie_len;

    /* 填充Extended Capabilities信息 */
    mac_set_ext_capabilities_ie(&(pst_dmac_vap->st_vap_base_info), puc_payload_addr, &uc_ie_len);
    puc_payload_addr += uc_ie_len;

    /* 填充vht capabilities信息 */
    mac_set_vht_capabilities_ie(&(pst_dmac_vap->st_vap_base_info), puc_payload_addr, &uc_ie_len);
    puc_payload_addr += uc_ie_len;

    /* 填充WPS信息 */
    mac_add_app_ie((oal_void *)&(pst_dmac_vap->st_vap_base_info), puc_payload_addr, &us_app_ie_len, OAL_APP_PROBE_REQ_IE);
    puc_payload_addr += us_app_ie_len;
#ifdef _PRE_WLAN_FEATURE_HILINK
    /* 填充okc ie信息 */
    mac_add_app_ie((oal_void *)&(pst_dmac_vap->st_vap_base_info), puc_payload_addr, &us_app_ie_len, OAL_APP_OKC_PROBE_IE);
    puc_payload_addr += us_app_ie_len;
#endif

#ifdef _PRE_WLAN_FEATURE_11K
    if (OAL_TRUE == pst_dmac_vap->bit_11k_enable)
    {
        mac_set_wfa_tpc_report_ie(&(pst_dmac_vap->st_vap_base_info), puc_payload_addr, &uc_ie_len);
        puc_payload_addr += uc_ie_len;
    }
#endif //_PRE_WLAN_FEATURE_11K

    return (oal_uint16)(puc_payload_addr - puc_payload_addr_origin + MAC_80211_FRAME_LEN);
}


oal_uint32  dmac_scan_send_probe_req_frame(dmac_vap_stru *pst_dmac_vap,
                                            oal_uint8 *puc_bssid,
                                            oal_int8 *pc_ssid)
{
    oal_netbuf_stru        *pst_mgmt_buf;
    mac_tx_ctl_stru        *pst_tx_ctl;
    oal_uint32              ul_ret;
    oal_uint16              us_mgmt_len;
    oal_uint8              *puc_mac_header = OAL_PTR_NULL;
    oal_uint8              *puc_saddr;

    /* 申请管理帧内存 */
    pst_mgmt_buf = OAL_MEM_NETBUF_ALLOC(OAL_MGMT_NETBUF, WLAN_MGMT_NETBUF_SIZE, OAL_NETBUF_PRIORITY_MID);
    if (OAL_PTR_NULL == pst_mgmt_buf)
    {
        OAM_ERROR_LOG0(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN, "{dmac_scan_send_probe_req_frame::alloc netbuf failed.}");
        OAL_MEM_INFO_PRINT(OAL_MEM_POOL_ID_NETBUF);
        return OAL_ERR_CODE_PTR_NULL;
    }

    oal_set_netbuf_prev(pst_mgmt_buf, OAL_PTR_NULL);
    oal_set_netbuf_next(pst_mgmt_buf, OAL_PTR_NULL);

    OAL_MEM_NETBUF_TRACE(pst_mgmt_buf, OAL_TRUE);

    /* 封装probe request帧 */
    us_mgmt_len = dmac_scan_encap_probe_req_frame(pst_dmac_vap, pst_mgmt_buf, puc_bssid, pc_ssid);

    /* 填写netbuf的cb字段，供发送管理帧和发送完成接口使用 */
    pst_tx_ctl = (mac_tx_ctl_stru *)oal_netbuf_cb(pst_mgmt_buf);

    OAL_MEMZERO(pst_tx_ctl, sizeof(mac_tx_ctl_stru));
    mac_set_cb_ac(pst_tx_ctl, WLAN_WME_AC_MGMT);

    if (!ETHER_IS_MULTICAST(puc_bssid))
    {
        /* 发送单播探测帧 */
        puc_mac_header = oal_netbuf_header(pst_mgmt_buf);
        puc_saddr = mac_vap_get_mac_addr(&(pst_dmac_vap->st_vap_base_info));
        oal_set_mac_addr(puc_mac_header + WLAN_HDR_ADDR1_OFFSET, puc_bssid);
        oal_set_mac_addr(puc_mac_header + WLAN_HDR_ADDR2_OFFSET, puc_saddr);
        MAC_GET_CB_IS_MCAST(pst_tx_ctl) = OAL_FALSE;
        MAC_GET_CB_TX_USER_IDX(pst_tx_ctl) =(oal_uint8)pst_dmac_vap->st_vap_base_info.uc_assoc_vap_id;
    }
    else
    {
        /* 发动广播探测帧 */
        MAC_GET_CB_IS_MCAST(pst_tx_ctl) = OAL_TRUE;
        MAC_GET_CB_TX_USER_IDX(pst_tx_ctl) =(oal_uint8)pst_dmac_vap->st_vap_base_info.us_multi_user_idx; /* probe request帧是广播帧 */
    }

    /* 调用发送管理帧接口 */
    ul_ret = dmac_tx_mgmt(pst_dmac_vap, pst_mgmt_buf, us_mgmt_len);
    if (OAL_SUCC != ul_ret)
    {

        oal_netbuf_free(pst_mgmt_buf);
        return ul_ret;
    }

    return OAL_SUCC;
}


oal_uint32  dmac_scan_proc_scan_complete_event(dmac_vap_stru *pst_dmac_vap,
                                               mac_scan_status_enum_uint8 en_scan_rsp_status)
{
    frw_event_mem_stru         *pst_event_mem;
    frw_event_stru             *pst_event;
    mac_device_stru            *pst_mac_device;
    oal_uint8                   uc_vap_id;
    mac_scan_rsp_stru          *pst_scan_rsp_info;

    uc_vap_id = pst_dmac_vap->st_vap_base_info.uc_vap_id;

    //OAM_INFO_LOG0(uc_vap_id, OAM_SF_SCAN, "{dmac_scan_proc_scan_complete_event:: start proc scan complete event.}");
    //dmac_scan_print_time_stamp();

    /* 获取device结构体 */
    pst_mac_device = mac_res_get_dev(pst_dmac_vap->st_vap_base_info.uc_device_id);
    if (OAL_PTR_NULL == pst_mac_device)
    {
        OAM_ERROR_LOG0(uc_vap_id, OAM_SF_SCAN, "{dmac_scan_proc_scan_complete_event::pst_mac_device null.}");
        return OAL_ERR_CODE_PTR_NULL;
    }

    /* 抛扫描请求事件到DMAC, 申请事件内存 */
    pst_event_mem = FRW_EVENT_ALLOC(OAL_SIZEOF(mac_scan_rsp_stru));
    if (OAL_PTR_NULL == pst_event_mem)
    {
        OAM_ERROR_LOG0(uc_vap_id, OAM_SF_SCAN, "{dmac_scan_proc_scan_complete_event::alloc memory failed.}");
        return OAL_ERR_CODE_PTR_NULL;
    }

    /* 填写事件 */
    pst_event = (frw_event_stru *)pst_event_mem->puc_data;

    FRW_EVENT_HDR_INIT(&(pst_event->st_event_hdr),
                       FRW_EVENT_TYPE_WLAN_CRX,
                       DMAC_WLAN_CRX_EVENT_SUB_TYPE_SCAN_COMP,
                       OAL_SIZEOF(mac_scan_rsp_stru),
                       FRW_EVENT_PIPELINE_STAGE_1,
                       pst_dmac_vap->st_vap_base_info.uc_chip_id,
                       pst_dmac_vap->st_vap_base_info.uc_device_id,
                       pst_dmac_vap->st_vap_base_info.uc_vap_id);

    pst_scan_rsp_info = (mac_scan_rsp_stru *)(pst_event->auc_event_data);

    /* 设置扫描完成时状态，是被拒绝，还是执行成功 */
    if(OAL_TRUE == pst_mac_device->st_scan_params.en_abort_scan_flag)
    {
        pst_scan_rsp_info->en_scan_rsp_status = MAC_SCAN_ABORT;
    }
    else
    {
        pst_scan_rsp_info->en_scan_rsp_status = en_scan_rsp_status;
    }
    pst_scan_rsp_info->ull_cookie         = pst_mac_device->st_scan_params.ull_cookie;

    OAM_WARNING_LOG4(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN, "{dmac_scan_proc_scan_complete_event::status:%d, vap channel:%d, cookie[%x], scan_mode[%d]}",
                        en_scan_rsp_status,
                        pst_dmac_vap->st_vap_base_info.st_channel.uc_chan_number,
                        pst_scan_rsp_info->ull_cookie,
                        pst_mac_device->st_scan_params.en_scan_mode);

    /* 分发事件 */
    frw_event_dispatch_event(pst_event_mem);
    FRW_EVENT_FREE(pst_event_mem);

    return OAL_SUCC;
}

#if 0

OAL_STATIC oal_uint32  dmac_scan_is_too_busy(mac_device_stru *pst_mac_device, mac_scan_req_stru *pst_scan_req_params)
{
    oal_uint32       ul_ret;
    oal_uint32       ul_timestamp;
    oal_uint32       ul_deltatime;

    ul_ret = mac_device_is_p2p_connected(pst_mac_device);
    if (OAL_SUCC == ul_ret)
    {
        ul_timestamp = (oal_uint32)OAL_TIME_GET_STAMP_MS();
        ul_deltatime = (ul_timestamp > pst_mac_device->ul_scan_timestamp)? \
                       (ul_timestamp - pst_mac_device->ul_scan_timestamp):(0xffffffff - pst_mac_device->ul_scan_timestamp + ul_timestamp);

        if (MAC_SCAN_FUNC_P2P_LISTEN != pst_scan_req_params->uc_scan_func)
        {
            if (ul_deltatime < DMAC_SCAN_DBAC_SCAN_DELTA_TIME)
            {
                OAM_WARNING_LOG2(0, OAM_SF_DBAC, "has connected p2p. scan deltatime:%d<%d, refused", ul_deltatime, DMAC_SCAN_DBAC_SCAN_DELTA_TIME);
                return OAL_TRUE;
            }
        }
        else
        {
            if (ul_deltatime > 500 || pst_scan_req_params->us_scan_time >= DMAC_SCAN_GO_MAX_SCAN_TIME)
            {
                OAM_WARNING_LOG2(0, OAM_SF_DBAC, "has connected p2p. p2p listen deltatime:%d, scan_time:%d, refused", ul_deltatime, pst_scan_req_params->us_scan_time);
                return OAL_TRUE;
            }
        }
    }

    return OAL_FALSE;
}
#endif


oal_uint32  dmac_scan_update_channel_list(mac_device_stru    *pst_mac_device,
                                        dmac_vap_stru      *pst_dmac_vap)
{
#ifdef _PRE_WLAN_FEATURE_PROXYSTA
    if (mac_is_proxysta_enabled(pst_mac_device))
    {
        /* proxysta 只扫描一个信道 */
        if (mac_vap_is_vsta(&pst_dmac_vap->st_vap_base_info))
        {
            oal_uint8 uc_idx;
            oal_int32 l_found_idx = -1;

            mac_vap_stru *pst_msta = mac_find_main_proxysta(pst_mac_device);

            if (!pst_msta)
            {
                return OAL_FAIL;
            }

            for (uc_idx = 0; uc_idx < pst_mac_device->st_scan_params.uc_channel_nums; uc_idx++)
            {
                if (pst_mac_device->st_scan_params.ast_channel_list[uc_idx].uc_chan_number == pst_msta->st_channel.uc_chan_number)
                {
                    l_found_idx = (oal_int32)uc_idx;
                    break;
                }
            }

            if (l_found_idx >= 0)
            {
                pst_mac_device->st_scan_params.uc_channel_nums = 1;
                pst_mac_device->st_scan_params.ast_channel_list[0] = pst_mac_device->st_scan_params.ast_channel_list[l_found_idx];
            }
        }
    }
#endif
    return OAL_SUCC;
}


oal_uint32  dmac_scan_handle_scan_req_entry(mac_device_stru    *pst_mac_device,
                                            dmac_vap_stru      *pst_dmac_vap,
                                            mac_scan_req_stru  *pst_scan_req_params)
{
#ifdef _PRE_WLAN_FEATURE_BTCOEX
    hal_to_dmac_device_stru *pst_hal_device = OAL_PTR_NULL;
#endif
    /* 如果处于扫描状态，则直接返回 */
    /* 如果处于常发常收状态，则直接返回 */
    if((MAC_SCAN_STATE_RUNNING == pst_mac_device->en_curr_scan_state)
       || ((OAL_SWITCH_ON == pst_dmac_vap->st_vap_base_info.bit_al_tx_flag) && (WLAN_VAP_MODE_BSS_STA == pst_dmac_vap->st_vap_base_info.en_vap_mode)))
    {
    #if (defined(_PRE_PRODUCT_ID_HI110X_DEV))
        OAM_WARNING_LOG_ALTER(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN,
                         "{dmac_scan_handle_scan_req_entry:: device scan is running or always tx is running, cann't start scan. scan_vap_id[%d], scan_func[0x%02x], curr_scan_mode[%d], req_scan_mode[%d], scan_cookie[%x], al_tx_flag[%d].}",
                         6,
                         pst_mac_device->st_scan_params.uc_vap_id,
                         pst_mac_device->st_scan_params.uc_scan_func,
                         pst_mac_device->st_scan_params.en_scan_mode,
                         pst_scan_req_params->en_scan_mode,
                         pst_scan_req_params->ull_cookie,
                         pst_dmac_vap->st_vap_base_info.bit_al_tx_flag);
    #else
        OAM_WARNING_LOG4(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN,
                         "{dmac_scan_handle_scan_req_entry:: device scan is running or always tx is running, cann't start scan. scan_vap_id[%d], scan_func[0x%02x], scan_cookie[%x], al_tx_flag[%d].}",
                         pst_mac_device->st_scan_params.uc_vap_id,
                         pst_mac_device->st_scan_params.uc_scan_func,
                         pst_scan_req_params->ull_cookie,
                         pst_dmac_vap->st_vap_base_info.bit_al_tx_flag);
    #endif

        /* 如果是上层下发的扫描请求，直接抛扫描完成事件; OBSS扫描则返回结束，等待下一次定时器超时再发起扫描 */
        if (pst_scan_req_params->en_scan_mode < WLAN_SCAN_MODE_BACKGROUND_OBSS)
        {
            /* 更新扫描下发的cookie 值 */
            pst_mac_device->st_scan_params.ull_cookie = pst_scan_req_params->ull_cookie;

            /* 抛扫描完成事件，扫描请求被拒绝 */
            return dmac_scan_proc_scan_complete_event(pst_dmac_vap, MAC_SCAN_REFUSED);
        }
#ifdef _PRE_WLAN_FEATURE_11K
        else if (WLAN_SCAN_MODE_RRM_BEACON_REQ == pst_scan_req_params->en_scan_mode)
        {
            OAM_ERROR_LOG0(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN,
                         "{dmac_scan_handle_scan_req_entry:: RRM BEACON REQ SCAN FAIL");
            return  OAL_FAIL;
        }
#endif
        else
        {
            return OAL_SUCC;
        }
    }

#if defined(_PRE_PRODUCT_ID_HI110X_DEV)
    pst_mac_device->uc_scan_ap_num_in_2p4                 = 0;

    if (WLAN_SCAN_MODE_BACKGROUND_OBSS > pst_scan_req_params->en_scan_mode)
    {
        pst_mac_device->uc_scan_count++;
    }
#endif

    /* 设置device当前扫描状态为运行状态 */
    if (WLAN_SCAN_MODE_BACKGROUND_CCA == pst_scan_req_params->en_scan_mode)
    {
        /* 如果是算法发起CCA 扫描，不能设置en_curr_scan_state 为扫描状态，避免和host 发起的扫描冲突 */
        pst_mac_device->en_curr_scan_state = MAC_SCAN_STATE_IDLE;
    }
    else
    {
        pst_mac_device->en_curr_scan_state = MAC_SCAN_STATE_RUNNING;

        /* 入网开始，通知BT */
#ifdef _PRE_WLAN_FEATURE_BTCOEX
        hal_set_btcoex_soc_gpreg1(OAL_TRUE, BIT1, 1);   // 入网流程开始
        hal_coex_sw_irq_set(HAL_COEX_SW_IRQ_BT);
#endif

#ifdef _PRE_WLAN_FEATURE_STA_PM
        /* 平台的计数 */
        if (WLAN_SCAN_MODE_BACKGROUND_AP >= pst_scan_req_params->en_scan_mode)
        {
            PM_WLAN_DumpSleepCnt();
        }
#endif
    }
    pst_mac_device->ul_scan_timestamp  = (oal_uint32)OAL_TIME_GET_STAMP_MS();

    /* 竞争到扫描权限后，将扫描参数拷贝到mac deivce结构体下，此时拷贝，也是为了防止扫描参数被覆盖情况 */
    oal_memcopy(&(pst_mac_device->st_scan_params), pst_scan_req_params, OAL_SIZEOF(mac_scan_req_stru));
    pst_mac_device->en_scan_curr_chan_find_bss_flag = OAL_FALSE;
    pst_mac_device->st_scan_params.uc_curr_channel_scan_count = 0;
#if defined(_PRE_PRODUCT_ID_HI110X_DEV)
    pst_mac_device->st_scan_params.uc_last_channel_band   = WLAN_BAND_BUTT;
#endif

#ifdef _PRE_WLAN_FEATURE_P2P
    /* P2P0 扫描时记录P2P listen channel */
    if (OAL_TRUE == pst_scan_req_params->bit_is_p2p0_scan)
    {
        pst_dmac_vap->st_vap_base_info.uc_p2p_listen_channel = pst_scan_req_params->uc_p2p0_listen_channel;
    }
#endif
    /* 初始化扫描信道索引 */
    pst_mac_device->uc_scan_chan_idx = 0;
    dmac_scan_update_channel_list(pst_mac_device, pst_dmac_vap);

#ifdef _PRE_WLAN_FEATURE_PROXYSTA
    if (mac_is_proxysta_enabled(pst_mac_device)
      && pst_mac_device->st_scan_params.uc_channel_nums == 1
      && pst_mac_device->st_scan_params.ast_channel_list[0].uc_chan_number == pst_mac_device->pst_device_stru->uc_current_chan_number)
    {
        OAM_INFO_LOG1(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN,"{dmac_scan_handle_scan_req_entry:proxysta scan working channel %d, skip switching}",
                pst_mac_device->st_scan_params.ast_channel_list[0].uc_chan_number);
    }
    else
#endif
    {
        dmac_scan_switch_channel_off(pst_mac_device);
    }

    /* 扫描开始时，根据是否是随机扫描，重新设置vap的mac addr */
    dmac_scan_set_vap_mac_addr_by_scan_state(pst_mac_device, OAL_TRUE);

    /* 清空信道测量结果 */
    OAL_MEMZERO(&(pst_mac_device->st_chan_result), OAL_SIZEOF(mac_scan_chan_stats_stru));
#ifdef _PRE_WLAN_FEATURE_BTCOEX
    pst_hal_device = pst_mac_device->pst_device_stru;
    if (OAL_PTR_NULL == pst_hal_device)
    {
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "dmac_btcoex_scan_begin:pst_hal_device is null");
        return OAL_ERR_CODE_PTR_NULL;
    }
    if(HAL_BTCOEX_SW_POWSAVE_WORK == GET_HAL_BTCOEX_SW_PREEMPT_TYPE(pst_hal_device))
    {
        GET_HAL_BTCOEX_SW_PREEMPT_TYPE(pst_hal_device) = HAL_BTCOEX_SW_POWSAVE_SCAN_BEGIN;
        OAM_WARNING_LOG0(0, OAM_SF_COEX, "{dmac_scan_handle_scan_req_entry:: normal scan begin delay by btcoex!}");
    }
    else
#endif
    {
        dmac_scan_begin(pst_mac_device);
    }
    return OAL_SUCC;
}


OAL_STATIC oal_void dmac_scan_prepare_pno_scan_params(mac_scan_req_stru  *pst_scan_params,
                                                                  dmac_vap_stru    *pst_dmac_vap)
{
    oal_uint8           uc_chan_idx;
    oal_uint8           uc_2g_chan_num = 0;
    oal_uint8           uc_5g_chan_num = 0;
    oal_uint8           uc_chan_number;
    mac_device_stru    *pst_mac_device;

    /* 扫描请求参数清零 */
    OAL_MEMZERO(pst_scan_params, OAL_SIZEOF(mac_scan_req_stru));

    /* 设置发起扫描的vap id */
    pst_scan_params->uc_vap_id = pst_dmac_vap->st_vap_base_info.uc_vap_id;

    /* 获取mac device */
    pst_mac_device = mac_res_get_dev(pst_dmac_vap->st_vap_base_info.uc_device_id);
    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_mac_device))
    {
        OAM_ERROR_LOG0(pst_dmac_vap->st_vap_base_info.uc_vap_id,OAM_SF_SCAN, "{dmac_scan_prepare_pno_scan_params::pst_mac_device null.}");
        return;
    }

    /* 设置初始扫描请求的参数 */
    pst_scan_params->en_bss_type    = WLAN_MIB_DESIRED_BSSTYPE_INFRA;
    pst_scan_params->en_scan_type   = WLAN_SCAN_TYPE_ACTIVE;
    pst_scan_params->en_scan_mode   = WLAN_SCAN_MODE_BACKGROUND_PNO;
    pst_scan_params->us_scan_time   = WLAN_DEFAULT_ACTIVE_SCAN_TIME * 2; /* 考虑PNO指定SSID扫描,延长每信道扫描时间为40ms */
    pst_scan_params->uc_probe_delay = 0;
    pst_scan_params->uc_scan_func   = MAC_SCAN_FUNC_BSS;   /* 默认扫描bss */
    pst_scan_params->uc_max_scan_count_per_channel           = 1;
    pst_scan_params->uc_max_send_probe_req_count_per_channel = WLAN_DEFAULT_SEND_PROBE_REQ_COUNT_PER_CHANNEL;

	/* 设置扫描用的pro req的src mac地址*/
    oal_set_mac_addr(pst_scan_params->auc_sour_mac_addr, pst_mac_device->pst_pno_sched_scan_mgmt->st_pno_sched_scan_params.auc_sour_mac_addr);
    pst_scan_params->en_is_random_mac_addr_scan = pst_mac_device->pst_pno_sched_scan_mgmt->st_pno_sched_scan_params.en_is_random_mac_addr_scan;

    /* 设置扫描请求的ssid信息 */
    pst_scan_params->ast_mac_ssid_set[0].auc_ssid[0] = '\0';   /* 通配ssid */
    pst_scan_params->uc_ssid_num = 1;

    /* 设置扫描请求只指定1个bssid，为广播地址 */
    oal_set_mac_addr(pst_scan_params->auc_bssid[0], BROADCAST_MACADDR);
    pst_scan_params->uc_bssid_num = 1;

    /* 2G初始扫描信道, 全信道扫描 */
    for (uc_chan_idx = 0; uc_chan_idx < MAC_CHANNEL_FREQ_2_BUTT; uc_chan_idx++)
    {
        /* 判断信道是不是在管制域内 */
        if (OAL_SUCC == mac_is_channel_idx_valid(WLAN_BAND_2G, uc_chan_idx))
        {
            mac_get_channel_num_from_idx(WLAN_BAND_2G, uc_chan_idx, &uc_chan_number);

            pst_scan_params->ast_channel_list[uc_2g_chan_num].uc_chan_number = uc_chan_number;
            pst_scan_params->ast_channel_list[uc_2g_chan_num].en_band        = WLAN_BAND_2G;
            pst_scan_params->ast_channel_list[uc_2g_chan_num].uc_idx         = uc_chan_idx;
            pst_scan_params->uc_channel_nums++;
            uc_2g_chan_num++;
        }
    }
    //OAM_INFO_LOG1(0, OAM_SF_SCAN, "{dmac_scan_prepare_pno_scan_params::after regdomain filter, the 2g total channel num is %d", uc_2g_chan_num);
#ifdef _PRE_PLAT_FEATURE_CUSTOMIZE
    if (OAL_TRUE != g_st_customize.st_cali.uc_band_5g_enable)
    {
        return;
    }
#endif

    /* 5G初始扫描信道, 全信道扫描 */
    for (uc_chan_idx = 0; uc_chan_idx < MAC_CHANNEL_FREQ_5_BUTT; uc_chan_idx++)
    {
        /* 判断信道是不是在管制域内 */
        if (OAL_SUCC == mac_is_channel_idx_valid(WLAN_BAND_5G, uc_chan_idx))
        {
            mac_get_channel_num_from_idx(WLAN_BAND_5G, uc_chan_idx, &uc_chan_number);

            pst_scan_params->ast_channel_list[uc_2g_chan_num + uc_5g_chan_num].uc_chan_number = uc_chan_number;
            pst_scan_params->ast_channel_list[uc_2g_chan_num + uc_5g_chan_num].en_band        = WLAN_BAND_5G;
            pst_scan_params->ast_channel_list[uc_2g_chan_num + uc_5g_chan_num].uc_idx         = uc_chan_idx;
            pst_scan_params->uc_channel_nums++;
            uc_5g_chan_num++;
        }
    }
    //OAM_INFO_LOG1(0, OAM_SF_SCAN, "{dmac_scan_prepare_pno_scan_params::after regdomain filter, the 5g total channel num is %d", uc_5g_chan_num);

    return;
}

#if (_PRE_MULTI_CORE_MODE_OFFLOAD_DMAC == _PRE_MULTI_CORE_MODE)

/*lint -e528*/
OAL_STATIC oal_void  dmac_scan_pno_scan_timeout_fn(void *p_ptr, void *p_arg)
{
    dmac_vap_stru                       *pst_dmac_vap;
    mac_device_stru                     *pst_mac_device;
    mac_scan_req_stru                    st_scan_req_params;


    pst_dmac_vap = (dmac_vap_stru *)p_arg;
    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_dmac_vap))
    {
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "{dmac_scan_pno_scan_timeout_fn::pst_dmac_vap null.}");
        return;
    }

    /* 获取mac device */
    pst_mac_device = mac_res_get_dev(pst_dmac_vap->st_vap_base_info.uc_device_id);
    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_mac_device))
    {
        OAM_ERROR_LOG0(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN,
                       "{dmac_scan_pno_scan_timeout_fn::pst_mac_device null.}");
        return;
    }

#ifdef _PRE_WLAN_FEATURE_STA_PM
    PM_WLAN_PsmHandle(PM_STA0_ID, PM_WLAN_WORK_PROCESS);
#endif


#if 0
    /* 更新pno调度扫描的次数 */
    pst_mac_device->pst_pno_sched_scan_mgmt->uc_curr_pno_sched_scan_times++;

    /* pno调度扫描到达最大重复次数，停止扫描进入低功耗 */
    if (pst_mac_device->pst_pno_sched_scan_mgmt->uc_curr_pno_sched_scan_times >= pst_mac_device->pst_pno_sched_scan_mgmt->st_pno_sched_scan_params.uc_pno_scan_repeat)
    {
        /* 停止PNO扫描，并进入低功耗 */
        OAM_WARNING_LOG0(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN,
                       "{dmac_scan_pno_scan_timeout_fn:: reached max pno scan repeat times, stop pno sched scan.}");

#ifdef _PRE_WLAN_FEATURE_STA_PM
        dmac_pm_sta_post_event(pst_dmac_vap, STA_PWR_EVENT_PNO_SCHED_SCAN_COMP, 0, OAL_PTR_NULL);
#endif
        /* 释放PNO管理结构体内存 */
        OAL_MEM_FREE(pst_mac_device->pst_pno_sched_scan_mgmt, OAL_TRUE);
        pst_mac_device->pst_pno_sched_scan_mgmt = OAL_PTR_NULL;
        return OAL_SUCC;
    }
#endif

    /* 初始化设置为: 未扫描到匹配的bss */
    pst_mac_device->pst_pno_sched_scan_mgmt->en_is_found_match_ssid = OAL_FALSE;

    /* 准备PNO扫描参数，准备发起扫描 */
    dmac_scan_prepare_pno_scan_params(&st_scan_req_params, pst_dmac_vap);

    /* 设置帧过滤寄存器 */
    hal_set_rx_filter(pst_mac_device->pst_device_stru, 0x37B9CEEA);

    /* 重新发起扫描 */
    dmac_scan_handle_scan_req_entry(pst_mac_device, pst_dmac_vap, &st_scan_req_params);

    /* 重新启动PNO调度扫描定时器 */
    dmac_scan_start_pno_sched_scan_timer((void *)pst_dmac_vap);

    return;
}
#endif



OAL_STATIC oal_uint32  dmac_scan_start_pno_sched_scan_timer(void *p_arg)
{
#if ((_PRE_MULTI_CORE_MODE_OFFLOAD_DMAC == _PRE_MULTI_CORE_MODE) && (_PRE_TEST_MODE != _PRE_TEST_MODE_UT))
    dmac_vap_stru                           *pst_dmac_vap = (dmac_vap_stru *)p_arg;
    mac_device_stru                         *pst_mac_device;
    mac_pno_sched_scan_mgmt_stru            *pst_pno_mgmt;
    oal_int32                                l_ret = OAL_SUCC;

    /* 获取mac device */
    pst_mac_device = mac_res_get_dev(pst_dmac_vap->st_vap_base_info.uc_device_id);
    if (OAL_PTR_NULL == pst_mac_device)
    {
        OAM_ERROR_LOG0(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN,
                       "{dmac_scan_start_pno_sched_scan_timer:: pst_mac_device is null.}");
        return OAL_FAIL;
    }

    /* 获取pno管理结构体 */
    pst_pno_mgmt = pst_mac_device->pst_pno_sched_scan_mgmt;

#if (_PRE_MULTI_CORE_MODE_OFFLOAD_DMAC == _PRE_MULTI_CORE_MODE)
    /* 判断pno调度扫描的rtc时钟定时器是否已经创建，如果未创建，则重新创建，否则，直接启动即可 */
    if (OAL_PTR_NULL == pst_pno_mgmt->p_pno_sched_scan_timer)
    {
        pst_pno_mgmt->p_pno_sched_scan_timer = (oal_void *)oal_rtctimer_create(dmac_scan_pno_scan_timeout_fn, p_arg);
        if (OAL_PTR_NULL == pst_pno_mgmt->p_pno_sched_scan_timer)
        {
            OAM_ERROR_LOG0(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN,
                           "{dmac_scan_start_pno_sched_scan_timer:: create pno timer faild.}");
            return OAL_FAIL;
        }
    }

#endif

    /* 参数合法性检查，时间间隔过短，pno调度只执行一次 */
    if (0 == pst_pno_mgmt->st_pno_sched_scan_params.ul_pno_scan_interval / 100)
    {
        OAM_WARNING_LOG1(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN,
                         "{dmac_scan_start_pno_sched_scan_timer:: pno scan interval[%d] time too short.}",
                         pst_pno_mgmt->st_pno_sched_scan_params.ul_pno_scan_interval);
        return OAL_FAIL;
    }

    /* 启动定时器，上层下发的扫描间隔是毫秒级的，而定时器是100毫秒级的，因此需要除以100 */
    l_ret = oal_rtctimer_start((STIMER_STRU *)pst_pno_mgmt->p_pno_sched_scan_timer, pst_pno_mgmt->st_pno_sched_scan_params.ul_pno_scan_interval / 100);
    if (OAL_SUCC != l_ret)
    {
        OAM_ERROR_LOG1(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN,
                         "{dmac_scan_start_pno_sched_scan_timer:: start pno timer faild[%d].}",l_ret);
        return OAL_FAIL;
    }

    OAM_WARNING_LOG1(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN,
                     "{dmac_scan_start_pno_sched_scan_timer:: start pno timer succ, timeout[%d].}",
                     pst_pno_mgmt->st_pno_sched_scan_params.ul_pno_scan_interval / 100);

    /* 当前PNO扫描定时器上层初始化是60s(PNO_SCHED_SCAN_INTERVAL),定时器到期后每次再延长60秒,最长不超过300s */
    pst_pno_mgmt->st_pno_sched_scan_params.ul_pno_scan_interval += (60 * 1000);
    if(pst_pno_mgmt->st_pno_sched_scan_params.ul_pno_scan_interval > (300 * 1000))
    {
        pst_pno_mgmt->st_pno_sched_scan_params.ul_pno_scan_interval = (300 * 1000);
    }

#else
    /* 1151不支持，且不会走到此处，do nothing，主要原因平台并未封装定时器相关接口 */
#endif

    return OAL_SUCC;
}


oal_uint32  dmac_scan_stop_pno_sched_scan_timer(void *p_arg)
{
#if ((_PRE_MULTI_CORE_MODE_OFFLOAD_DMAC == _PRE_MULTI_CORE_MODE) && (_PRE_TEST_MODE != _PRE_TEST_MODE_UT))
    mac_pno_sched_scan_mgmt_stru    *pst_pno_mgmt;

    pst_pno_mgmt = (mac_pno_sched_scan_mgmt_stru *)p_arg;

    /* 如果定时器不存在，直接返回 */
    if (OAL_PTR_NULL == pst_pno_mgmt->p_pno_sched_scan_timer)
    {
        OAM_WARNING_LOG0(0, OAM_SF_SCAN,
                         "{dmac_scan_stop_pno_sched_scan_timer:: pno sched timer not create yet.}");
        return OAL_SUCC;
    }
    /* 删除定时器 */
    oal_rtctimer_delete((STIMER_STRU *)pst_pno_mgmt->p_pno_sched_scan_timer);
    pst_pno_mgmt->p_pno_sched_scan_timer = OAL_PTR_NULL;
#else
    /* 1151不支持，且不会走到此处，do nothing，主要原因平台并未封装定时器相关接口 */
#endif

    return OAL_SUCC;
}


oal_uint32  dmac_scan_proc_sched_scan_req_event(frw_event_mem_stru *pst_event_mem)
{
    frw_event_stru             *pst_event;
    frw_event_hdr_stru         *pst_event_hdr;
    dmac_vap_stru              *pst_dmac_vap;
    mac_device_stru            *pst_mac_device;
    mac_scan_req_stru           st_scan_req_params;

    OAL_IO_PRINT("dmac_scan_proc_sched_scan_req_event::enter here.\r\n");

    /* 参数合法性检查 */
    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_event_mem))
    {
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "{dmac_scan_proc_sched_scan_req_event::pst_event_mem null.}");
        return OAL_ERR_CODE_PTR_NULL;
    }

    /* 获取事件信息 */
    pst_event        = (frw_event_stru *)(pst_event_mem->puc_data);
    pst_event_hdr    = &(pst_event->st_event_hdr);

    /* 获取dmac vap */
    pst_dmac_vap = (dmac_vap_stru *)mac_res_get_dmac_vap(pst_event_hdr->uc_vap_id);
    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_dmac_vap))
    {
        OAM_ERROR_LOG0(pst_event_hdr->uc_vap_id, OAM_SF_SCAN, "{dmac_scan_proc_sched_scan_req_event::pst_dmac_vap null.}");
        return OAL_ERR_CODE_PTR_NULL;
    }

    /* 获取mac device */
    pst_mac_device = mac_res_get_dev(pst_event_hdr->uc_device_id);
    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_mac_device))
    {
        OAM_ERROR_LOG0(pst_event_hdr->uc_vap_id, OAM_SF_SCAN, "{dmac_scan_proc_sched_scan_req_event::pst_mac_device null.}");
        return OAL_ERR_CODE_PTR_NULL;
    }

    /* 准备pno扫描请求参数 */
    dmac_scan_prepare_pno_scan_params(&st_scan_req_params, pst_dmac_vap);

    /* 设置PNO调度扫描的次数为0 */
    pst_mac_device->pst_pno_sched_scan_mgmt->uc_curr_pno_sched_scan_times = 0;
    pst_mac_device->pst_pno_sched_scan_mgmt->en_is_found_match_ssid = OAL_FALSE;

    /* 设置帧过滤寄存器 */
    hal_set_rx_filter(pst_mac_device->pst_device_stru, 0x37B9CEEA);

#ifdef _PRE_WLAN_FEATURE_STA_PM
	/* pno第一次扫描前必须先投一个work票,否则此次扫描失败且会造成扫描状态位错误 */
    PM_WLAN_PsmHandle(PM_STA0_ID, PM_WLAN_WORK_PROCESS);
#endif

    /* 调用扫描入口，执行扫描 */
    dmac_scan_handle_scan_req_entry(pst_mac_device, pst_dmac_vap, &st_scan_req_params);

    /* 启动pno调度扫描的rtc时钟定时器，可唤醒深睡的device */
    dmac_scan_start_pno_sched_scan_timer((void *)pst_dmac_vap);

    return OAL_SUCC;
}


oal_uint32  dmac_scan_proc_scan_req_event(frw_event_mem_stru *pst_event_mem)
{
    frw_event_stru             *pst_event;
    frw_event_hdr_stru         *pst_event_hdr;
    dmac_vap_stru              *pst_dmac_vap;
    mac_device_stru            *pst_mac_device;
    mac_scan_req_stru          *pst_h2d_scan_req_params;

    /* 参数合法性检查 */
    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_event_mem))
    {
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "{dmac_scan_proc_scan_req_event::pst_event_mem null.}");
        return OAL_ERR_CODE_PTR_NULL;
    }

    /* 获取事件信息 */
    pst_event        = (frw_event_stru *)(pst_event_mem->puc_data);
    pst_event_hdr    = &(pst_event->st_event_hdr);

    pst_dmac_vap = (dmac_vap_stru *)mac_res_get_dmac_vap(pst_event_hdr->uc_vap_id);
    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_dmac_vap))
    {
        OAM_ERROR_LOG0(pst_event_hdr->uc_vap_id, OAM_SF_SCAN, "{dmac_scan_proc_scan_req_event::pst_dmac_vap null.}");
        return OAL_ERR_CODE_PTR_NULL;
    }

    /* 获取mac device */
    pst_mac_device = mac_res_get_dev(pst_event_hdr->uc_device_id);
    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_mac_device))
    {
        OAM_ERROR_LOG0(pst_event_hdr->uc_vap_id, OAM_SF_SCAN, "{dmac_scan_proc_scan_req_event::pst_mac_device null.}");
        return OAL_ERR_CODE_PTR_NULL;
    }

    /* 获取到扫描请求参数 */
    pst_h2d_scan_req_params = (mac_scan_req_stru *)frw_get_event_payload(pst_event_mem);

    /* 保存随机mac扫描开关，用于dmac发起的随机扫描 */
    pst_mac_device->en_is_random_mac_addr_scan = pst_h2d_scan_req_params->en_is_random_mac_addr_scan;

    /* host侧发起的扫描请求的处理 */
    return dmac_scan_handle_scan_req_entry(pst_mac_device, pst_dmac_vap, pst_h2d_scan_req_params);
}

#ifdef _PRE_WLAN_FEATURE_20_40_80_COEXIST


OAL_STATIC oal_uint32 dmac_scan_prepare_obss_scan_params(mac_scan_req_stru  *pst_scan_params,
                                                         dmac_vap_stru      *pst_dmac_vap)
{
    mac_device_stru *pst_mac_device;
    oal_uint8        uc_2g_chan_num      = 0;
    oal_uint8        uc_channel_idx      = 0;
    oal_uint8        uc_low_channel_idx  = 0;
    oal_uint8        uc_high_channel_idx = 0;
    oal_uint8        uc_channel_num      = 0;
    oal_uint8        uc_curr_channel_num = pst_dmac_vap->st_vap_base_info.st_channel.uc_chan_number;
    oal_uint8        uc_curr_band        = pst_dmac_vap->st_vap_base_info.st_channel.en_band;

    /* 获取mac device */
    pst_mac_device = mac_res_get_dev(pst_dmac_vap->st_vap_base_info.uc_device_id);
    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_mac_device))
    {
        OAM_ERROR_LOG0(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN,
                       "{dmac_scan_prepare_obss_scan_params::mac_res_get_dev failed.}");
        return OAL_FAIL;
    }

    OAL_MEMZERO(pst_scan_params, OAL_SIZEOF(mac_scan_req_stru));

    /* 1.设置发起扫描的vap id */
    pst_scan_params->uc_vap_id = pst_dmac_vap->st_vap_base_info.uc_vap_id;

    /* 2.设置初始扫描请求的参数 */
    pst_scan_params->en_bss_type         = WLAN_MIB_DESIRED_BSSTYPE_INFRA;
    pst_scan_params->en_scan_type        = WLAN_SCAN_TYPE_ACTIVE;
    pst_scan_params->uc_probe_delay      = 0;
    pst_scan_params->uc_scan_func        = MAC_SCAN_FUNC_BSS;               /* 默认扫描bss */
    pst_scan_params->uc_max_send_probe_req_count_per_channel = WLAN_DEFAULT_SEND_PROBE_REQ_COUNT_PER_CHANNEL;
    pst_scan_params->uc_max_scan_count_per_channel           = 1;

    /* 根据扫描类型，确定每信道扫描时间 */
    if (WLAN_SCAN_TYPE_ACTIVE == pst_scan_params->en_scan_type)
    {
        pst_scan_params->us_scan_time = WLAN_DEFAULT_ACTIVE_SCAN_TIME;
    }
    else
    {
        pst_scan_params->us_scan_time = WLAN_DEFAULT_PASSIVE_SCAN_TIME;
    }

    /* OBSS扫描通配ssid */
    pst_scan_params->ast_mac_ssid_set[0].auc_ssid[0] = '\0';
    pst_scan_params->uc_ssid_num = 1;
    /* OBSS扫描设置Source MAC ADDRESS */
    if((pst_mac_device->en_is_random_mac_addr_scan)
       && ((pst_mac_device->auc_mac_oui[0] != 0) || (pst_mac_device->auc_mac_oui[1] != 0) || (pst_mac_device->auc_mac_oui[2] != 0)))
    {
        pst_scan_params->auc_sour_mac_addr[0] = (pst_mac_device->auc_mac_oui[0] & 0xfe);  /*保证是单播mac*/
        pst_scan_params->auc_sour_mac_addr[1] = pst_mac_device->auc_mac_oui[1];
        pst_scan_params->auc_sour_mac_addr[2] = pst_mac_device->auc_mac_oui[2];
        pst_scan_params->auc_sour_mac_addr[3] = oal_gen_random((oal_uint32)OAL_TIME_GET_STAMP_MS(), 1);
        pst_scan_params->auc_sour_mac_addr[4] = oal_gen_random((oal_uint32)OAL_TIME_GET_STAMP_MS(), 1);
        pst_scan_params->auc_sour_mac_addr[5] = oal_gen_random((oal_uint32)OAL_TIME_GET_STAMP_MS(), 1);
        pst_scan_params->en_is_random_mac_addr_scan = OAL_TRUE;
    }
    else
    {
        oal_set_mac_addr(pst_scan_params->auc_sour_mac_addr, pst_dmac_vap->st_vap_base_info.pst_mib_info->st_wlan_mib_sta_config.auc_dot11StationID);
    }

    /* OBSS扫描扫描只指定1个bssid，为广播地址 */
    oal_set_mac_addr(pst_scan_params->auc_bssid[0], BROADCAST_MACADDR);
    pst_scan_params->uc_bssid_num = 1;

    /* 设置扫描模式为OBSS扫描 */
    pst_scan_params->en_scan_mode = WLAN_SCAN_MODE_BACKGROUND_OBSS;

    /* 准备OBSS扫描的信道 */
    if (WLAN_BAND_2G == uc_curr_band)
    {

        /* 从当前信道左右偏移5个信道，计算OBSS扫描信道
           1) 当前信道idx小于等于5，则从0开始，到idx+5,
           2) 大于5小于8，应该从idx-5到idx+5,
           3) 大于8，则是从idx-5到13 */
        if (uc_curr_channel_num <= 5)
        {
            uc_low_channel_idx = 0;
            uc_high_channel_idx = uc_curr_channel_num + 5;
        }
        else if (5 < uc_curr_channel_num && uc_curr_channel_num <= 8)
        {
            uc_low_channel_idx  = uc_curr_channel_num - 5;
            uc_high_channel_idx = uc_curr_channel_num + 5;
        }
        else if (8 < uc_curr_channel_num && uc_curr_channel_num <= 13)
        {
            uc_low_channel_idx = uc_curr_channel_num - 5;
            uc_high_channel_idx = 13;
        }
        else
        {
            uc_low_channel_idx  = 0;
            uc_high_channel_idx = 0;
            OAM_ERROR_LOG1(0, OAM_SF_SCAN, "{dmac_scan_update_obss_scan_params::2040M,Current channel index is %d.}",
                           uc_curr_channel_num);
        }

        /* 准备2.4G下OBSS扫描信道 */
        for(uc_channel_idx = uc_low_channel_idx; uc_channel_idx <= uc_high_channel_idx; uc_channel_idx++)
        {
            /* 判断信道是不是在管制域内 */
            if (OAL_SUCC == mac_is_channel_idx_valid(WLAN_BAND_2G, uc_channel_idx))
            {
                mac_get_channel_num_from_idx(WLAN_BAND_2G, uc_channel_idx, &uc_channel_num);

                pst_scan_params->ast_channel_list[uc_2g_chan_num].uc_chan_number = uc_channel_num;
                pst_scan_params->ast_channel_list[uc_2g_chan_num].en_band        = WLAN_BAND_2G;
                pst_scan_params->ast_channel_list[uc_2g_chan_num].uc_idx         = uc_channel_idx;
                pst_scan_params->uc_channel_nums++;
                uc_2g_chan_num++;
            }
        }

        /* 更新本次扫描的信道总数 */
        pst_scan_params->uc_channel_nums = uc_2g_chan_num;
    }
#if 0
    else if (WLAN_BAND_5G == uc_curr_band)
    {
        /* 暂时不考虑5G下的obss扫描 */
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "{dmac_scan_update_obss_scan_params::5G don't do obss scan.}");
    }
    else
    {
        OAM_ERROR_LOG1(0, OAM_SF_SCAN, "{dmac_scan_update_obss_scan_params::error band[%d].}", uc_curr_band);
    }
#endif
    /* 如果当前扫描信道的总数为0，返回错误，不执行扫描请求 */
    if (0 == pst_scan_params->uc_channel_nums)
    {
        OAM_ERROR_LOG1(0, OAM_SF_SCAN, "{dmac_scan_update_obss_scan_params::scan total channel num is 0, band[%d]!}", uc_curr_band);
        return OAL_FAIL;
    }

    return OAL_SUCC;
}


OAL_STATIC oal_uint32  dmac_scan_obss_timeout_fn(void *p_arg)
{
    dmac_vap_stru          *pst_dmac_vap;
    mac_device_stru        *pst_mac_device;
    mac_scan_req_stru       st_scan_req_params;
    oal_uint32              ul_ret;
    oal_uint8               uc_scan_now = OAL_FALSE;

    //OAM_INFO_LOG0(0, OAM_SF_SCAN, "{dmac_scan_obss_timeout_fn::obss timer time out.}");

    pst_dmac_vap = (dmac_vap_stru *)p_arg;
    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_dmac_vap))
    {
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "{dmac_scan_obss_timeout_fn::pst_dmac_vap null.}");
        return OAL_FAIL;
    }

    /* 获取mac device */
    pst_mac_device = mac_res_get_dev(pst_dmac_vap->st_vap_base_info.uc_device_id);
    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_mac_device))
    {
        OAM_ERROR_LOG0(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN,
                       "{dmac_scan_obss_timeout_fn::pst_mac_device null.}");
        return OAL_FAIL;
    }

    /* 如果在obss scan timer启动期间动态修改了sta能力导致sta不支持obss扫描，
     * 则关闭obss scan timer
     * 此处是唯一中止obss扫描的地方!!!
     */
    if (OAL_FALSE == dmac_mgmt_need_obss_scan(&pst_dmac_vap->st_vap_base_info))
    {
        pst_dmac_vap->ul_obss_scan_timer_remain  = 0;
        pst_dmac_vap->uc_obss_scan_timer_started = OAL_FALSE;
        //OAM_INFO_LOG0(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN,
        //            "{dmac_scan_obss_timeout_fn::stop obss scan timer}");
        return OAL_SUCC;
    }

    if (0 == pst_dmac_vap->ul_obss_scan_timer_remain)
    {
        uc_scan_now = OAL_TRUE;
    }

    dmac_scan_start_obss_timer(&pst_dmac_vap->st_vap_base_info);

    /* 进入扫描入口，执行obss扫描 */
    if (OAL_TRUE == uc_scan_now)
    {
        /* 准备OBSS扫描参数，准备发起扫描 */
        ul_ret = dmac_scan_prepare_obss_scan_params(&st_scan_req_params, pst_dmac_vap);
        if (OAL_SUCC != ul_ret)
        {
            //OAM_WARNING_LOG1(0, OAM_SF_SCAN, "{dmac_scan_obss_timeout_fn::update scan params error[%d].}", ul_ret);
            return ul_ret;
        }

        OAM_WARNING_LOG0(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN,
                         "{dmac_scan_obss_timeout_fn:: start scan}");
        return dmac_scan_handle_scan_req_entry(pst_mac_device, pst_dmac_vap, &st_scan_req_params);
    }

    return OAL_SUCC;
}


oal_void dmac_scan_start_obss_timer(mac_vap_stru *pst_mac_vap)
{
    dmac_vap_stru                 *pst_dmac_vap;
    mac_device_stru               *pst_mac_device;
    oal_uint32                    ul_new_timer;

    /* 根据发起扫描的vap id获取dmac vap */
    pst_dmac_vap = mac_res_get_dmac_vap(pst_mac_vap->uc_vap_id);
    if (OAL_PTR_NULL == pst_dmac_vap)
    {
        OAM_WARNING_LOG0(0, OAM_SF_SCAN, "{dmac_scan_start_obss_timer:: pst_dmac_vap is NULL.}");
        return;
    }

    /* 获取mac device */
    pst_mac_device = mac_res_get_dev(pst_mac_vap->uc_device_id);
    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_mac_device))
    {
        OAM_ERROR_LOG0(pst_mac_vap->uc_vap_id, OAM_SF_SCAN,
                       "{dmac_scan_start_obss_timer::pst_mac_device null.}");
        return;
    }

    /* 重置扫描定时器 */
    if (0 == pst_dmac_vap->ul_obss_scan_timer_remain)
    {
        pst_dmac_vap->ul_obss_scan_timer_remain = 1000*mac_mib_get_BSSWidthTriggerScanInterval(&pst_dmac_vap->st_vap_base_info);
    }

    ul_new_timer = pst_dmac_vap->ul_obss_scan_timer_remain > DMAC_SCAN_MAX_TIMER?
                        DMAC_SCAN_MAX_TIMER:pst_dmac_vap->ul_obss_scan_timer_remain;
    pst_dmac_vap->ul_obss_scan_timer_remain -= ul_new_timer;
    OAM_INFO_LOG2(0, OAM_SF_SCAN, "{dmac_scan_start_obss_timer::remain=%d new_timer=%d}",
                pst_dmac_vap->ul_obss_scan_timer_remain, ul_new_timer);

    FRW_TIMER_CREATE_TIMER(&(pst_mac_device->st_obss_scan_timer),
                           dmac_scan_obss_timeout_fn,
                           ul_new_timer,
                           (void *)pst_dmac_vap,
                           OAL_FALSE,
                           OAM_MODULE_ID_DMAC,
                           pst_mac_device->ul_core_id);
    pst_dmac_vap->uc_obss_scan_timer_started = OAL_TRUE;

    return;
}
oal_uint8 g_csa_scan_flag = OAL_FALSE;

oal_uint32 dmac_trigger_csa_scan(mac_scan_req_stru  *pst_scan_params,
                                       mac_vap_stru      *pst_mac_vap,
                                       mac_channel_stru  *pst_old_channel)
{
    oal_uint8               uc_chan_num      = 0;
    oal_uint8               uc_channel_idx      = 0;
    dmac_vap_stru          *pst_dmac_vap;
    mac_device_stru        *pst_mac_device;

    if (pst_old_channel->uc_chan_number == pst_mac_vap->st_channel.uc_chan_number)
    {
        return OAL_SUCC;
    }

    OAM_WARNING_LOG0(0, OAM_SF_SCAN, "{dmac_trigger_csa_scan::update csa scan params.}");

    OAL_MEMZERO(pst_scan_params, OAL_SIZEOF(mac_scan_req_stru));

    /* 1.设置发起扫描的vap id */
    pst_scan_params->uc_vap_id = pst_mac_vap->uc_vap_id;

    /* 2.设置初始扫描请求的参数 */
    pst_scan_params->en_bss_type         = WLAN_MIB_DESIRED_BSSTYPE_INFRA;
    pst_scan_params->en_scan_type        = WLAN_SCAN_TYPE_ACTIVE;
    pst_scan_params->uc_probe_delay      = 0;
    pst_scan_params->uc_scan_func        = MAC_SCAN_FUNC_BSS;               /* 默认扫描bss */
    pst_scan_params->uc_max_send_probe_req_count_per_channel = WLAN_DEFAULT_SEND_PROBE_REQ_COUNT_PER_CHANNEL;
    pst_scan_params->uc_max_scan_count_per_channel           = 2;

    /* 根据扫描类型，确定每信道扫描时间 */
    if (WLAN_SCAN_TYPE_ACTIVE == pst_scan_params->en_scan_type)
    {
        pst_scan_params->us_scan_time = WLAN_DEFAULT_ACTIVE_SCAN_TIME;
    }

    /* CSA扫描通配ssid */
    pst_scan_params->ast_mac_ssid_set[0].auc_ssid[0] = '\0';
    pst_scan_params->uc_ssid_num = 1;
    /* CSA扫描设置Source MAC ADDRESS */
    oal_set_mac_addr(pst_scan_params->auc_sour_mac_addr, pst_mac_vap->pst_mib_info->st_wlan_mib_sta_config.auc_dot11StationID);

    /* CSA扫描扫描只指定1个bssid，为广播地址 */
    oal_set_mac_addr(pst_scan_params->auc_bssid[0], BROADCAST_MACADDR);
    pst_scan_params->uc_bssid_num = 1;

    /* 设置扫描模式为CSA扫描 */
    pst_scan_params->en_scan_mode = WLAN_SCAN_MODE_BACKGROUND_CSA;

    /* 准备扫描的信道 */
    /* 判断信道是不是在管制域内 */

    if (OAL_SUCC == mac_is_channel_num_valid(pst_old_channel->en_band, pst_old_channel->uc_chan_number))
    {
        mac_get_channel_idx_from_num(pst_old_channel->en_band, pst_old_channel->uc_chan_number, &uc_channel_idx);
        pst_scan_params->ast_channel_list[uc_chan_num].uc_chan_number = pst_old_channel->uc_chan_number;
        pst_scan_params->ast_channel_list[uc_chan_num].en_band        = pst_old_channel->en_band;
        pst_scan_params->ast_channel_list[uc_chan_num].uc_idx         = uc_channel_idx;
        pst_scan_params->uc_channel_nums++;
        uc_chan_num++;
    }

    if (OAL_SUCC == mac_is_channel_num_valid(pst_mac_vap->st_channel.en_band, pst_mac_vap->st_channel.uc_chan_number))
    {
        mac_get_channel_idx_from_num(pst_mac_vap->st_channel.en_band, pst_mac_vap->st_channel.uc_chan_number, &uc_channel_idx);
        pst_scan_params->ast_channel_list[uc_chan_num].uc_chan_number = pst_mac_vap->st_channel.uc_chan_number;
        pst_scan_params->ast_channel_list[uc_chan_num].en_band        = pst_mac_vap->st_channel.en_band;
        pst_scan_params->ast_channel_list[uc_chan_num].uc_idx         = uc_channel_idx;
        pst_scan_params->uc_channel_nums++;
        uc_chan_num++;
    }


    /* 更新本次扫描的信道总数 */
    pst_scan_params->uc_channel_nums = uc_chan_num;

    /* 如果当前扫描信道的总数为0，返回错误，不执行扫描请求 */
    if (0 == pst_scan_params->uc_channel_nums)
    {
       OAM_ERROR_LOG0(0, OAM_SF_SCAN, "{dmac_trigger_csa_scan::scan total channel num is 0.}");
       return OAL_FAIL;
    }

    pst_dmac_vap = (dmac_vap_stru *)mac_res_get_dmac_vap(pst_mac_vap->uc_vap_id);
    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_dmac_vap))
    {
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "{dmac_trigger_csa_scan::pst_dmac_vap null.}");
        return OAL_FAIL;
    }

    /* 获取mac device */
    pst_mac_device = mac_res_get_dev(pst_dmac_vap->st_vap_base_info.uc_device_id);
    if (OAL_UNLIKELY(OAL_PTR_NULL == pst_mac_device))
    {
        OAM_ERROR_LOG0(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN,
                       "{dmac_trigger_csa_scan::pst_mac_device null.}");
        return OAL_FAIL;
    }

    g_csa_scan_flag = OAL_TRUE;
    dmac_scan_handle_scan_req_entry(pst_mac_device, pst_dmac_vap, pst_scan_params);

    return OAL_SUCC;
}

#endif


OAL_STATIC oal_void  dmac_scan_switch_channel_notify_alg(dmac_vap_stru     *pst_dmac_vap,
                                                         mac_channel_stru  *pst_channel)
{
    mac_channel_stru                st_channel_tmp;

    /* 参数合法性检查 */
    if ((OAL_PTR_NULL == pst_dmac_vap) || (OAL_PTR_NULL == pst_channel))
    {
        OAM_ERROR_LOG2(0, OAM_SF_SCAN, "{dmac_scan_switch_channel_notify_alg::pst_dmac_vap[%p], pst_channel[%p].}",
                       pst_dmac_vap, pst_channel);
        return;
    }

    /* 记录当前vap下的信道信息 */
    st_channel_tmp = pst_dmac_vap->st_vap_base_info.st_channel;

    /* 记录要切换到信道的频段，切换信道 */
    pst_dmac_vap->st_vap_base_info.st_channel.en_band        = pst_channel->en_band;
    pst_dmac_vap->st_vap_base_info.st_channel.uc_chan_number = pst_channel->uc_chan_number;
    pst_dmac_vap->st_vap_base_info.st_channel.uc_idx         = pst_channel->uc_idx;
    pst_dmac_vap->st_vap_base_info.st_channel.en_bandwidth = WLAN_BAND_WIDTH_20M;

    /* 通知算法 */
    dmac_alg_cfg_channel_notify(&pst_dmac_vap->st_vap_base_info, CH_BW_CHG_TYPE_SCAN);
    dmac_alg_cfg_bandwidth_notify(&pst_dmac_vap->st_vap_base_info, CH_BW_CHG_TYPE_SCAN);

    /* 通知算法后，恢复vap原有信道信息 */
    pst_dmac_vap->st_vap_base_info.st_channel = st_channel_tmp;

    return;
}


oal_uint32  dmac_switch_channel_off(
                mac_device_stru     *pst_mac_device,
                mac_vap_stru        *pst_mac_vap,
                mac_channel_stru    *pst_dst_chl,
                oal_uint16           us_protect_time)
{
    mac_fcs_mgr_stru               *pst_fcs_mgr;
    mac_fcs_cfg_stru               *pst_fcs_cfg;

    /* 记录切离的信道，供扫描完后切回 */
    pst_mac_device->st_home_channel = pst_mac_vap->st_channel;

    pst_fcs_mgr = dmac_fcs_get_mgr_stru(pst_mac_device);
    pst_fcs_cfg = &(pst_mac_device->st_fcs_cfg);

    OAL_MEMZERO(pst_fcs_cfg, OAL_SIZEOF(mac_fcs_cfg_stru));

    pst_fcs_cfg->st_src_chl = pst_mac_vap->st_channel;
    pst_fcs_cfg->st_dst_chl = *pst_dst_chl;

    mac_fcs_prepare_one_packet_cfg(pst_mac_vap, &(pst_fcs_cfg->st_one_packet_cfg), us_protect_time);

    /* 调用FCS切信道接口 保存当前硬件队列的帧到扫描虚假队列 */
    mac_fcs_start(pst_fcs_mgr, pst_fcs_cfg, 0, HAL_TX_FAKE_QUEUE_BGSCAN_ID);
    mac_fcs_release(pst_fcs_mgr);

    OAM_WARNING_LOG2(pst_mac_vap->uc_vap_id, OAM_SF_SCAN, "{dmac_switch_channel_off::switch src channel[%d] to dst channel[%d].}",
                  pst_mac_vap->st_channel.uc_chan_number, pst_fcs_cfg->st_dst_chl.uc_chan_number);

    return OAL_SUCC;
}


oal_uint32  dmac_switch_channel_off_enhanced(
                mac_device_stru     *pst_mac_device,
                mac_vap_stru        *pst_mac_vap1,
                mac_vap_stru        *pst_mac_vap2,
                mac_channel_stru    *pst_dst_chl,
                oal_uint16           us_protect_time)
{
    mac_fcs_mgr_stru               *pst_fcs_mgr;
    mac_fcs_cfg_stru               *pst_fcs_cfg;
    mac_vap_stru                   *pst_vap_sta;
#ifdef _PRE_WLAN_FEATURE_DBAC
    mac_vap_stru                   *pst_current_chan_vap;   /* 当前信道上的VAP */
    oal_uint8                       uc_fake_q_id = 0;
#endif

    /* 记录切离时最大带宽的信道，供同频共存扫描完后切回 */
    if (pst_mac_vap1->st_channel.en_bandwidth >= pst_mac_vap2->st_channel.en_bandwidth)
    {
        pst_mac_device->st_home_channel = pst_mac_vap1->st_channel;
    }
    else
    {
        pst_mac_device->st_home_channel = pst_mac_vap2->st_channel;
    }

    /* 暂停两个VAP的发送 */
    dmac_vap_pause_tx(pst_mac_vap1);
    dmac_vap_pause_tx(pst_mac_vap2);

    pst_fcs_mgr = dmac_fcs_get_mgr_stru(pst_mac_device);
    pst_fcs_cfg = &(pst_mac_device->st_fcs_cfg);
    OAL_MEMZERO(pst_fcs_cfg, OAL_SIZEOF(mac_fcs_cfg_stru));

    pst_fcs_cfg->st_dst_chl = *pst_dst_chl;

    OAM_WARNING_LOG2(0, OAM_SF_DBAC, "{dmac_switch_channel_off_enhanced::curr hal chan[%d], dst channel[%d].}",
                  pst_mac_device->pst_device_stru->uc_current_chan_number,
                  pst_fcs_cfg->st_dst_chl.uc_chan_number);

#ifdef _PRE_WLAN_FEATURE_DBAC
    /* 异频模式只需要起一次one packet */
    if (mac_is_dbac_running(pst_mac_device))
    {
        /* 暂停DBAC切信道 */
        dmac_alg_dbac_pause(pst_mac_device);

        if (pst_mac_device->pst_device_stru->uc_current_chan_number == pst_mac_vap1->st_channel.uc_chan_number)
        {
            pst_current_chan_vap = pst_mac_vap1;
        }
        else
        {
            pst_current_chan_vap = pst_mac_vap2;
        }

        dmac_alg_get_dbac_fakeq_id(pst_current_chan_vap, &uc_fake_q_id);

        pst_fcs_cfg->st_src_chl = pst_current_chan_vap->st_channel;
        mac_fcs_prepare_one_packet_cfg(pst_current_chan_vap, &(pst_fcs_cfg->st_one_packet_cfg), us_protect_time);

        OAM_WARNING_LOG2(pst_current_chan_vap->uc_vap_id, OAM_SF_DBAC, "switch chan off when dbac running. curr chan num:%d, fake_q_id:%d",
                        pst_current_chan_vap->st_channel.uc_chan_number, uc_fake_q_id);
        if (pst_mac_device->pst_device_stru->uc_current_chan_number != pst_current_chan_vap->st_channel.uc_chan_number)
        {
            OAM_WARNING_LOG2(0, OAM_SF_DBAC, "switch chan off when dbac running. hal chan num:%d, curr vap chan num:%d. not same,do not send protect frame",
                            pst_mac_device->pst_device_stru->uc_current_chan_number,
                            pst_current_chan_vap->st_channel.uc_chan_number);

            pst_fcs_cfg->st_one_packet_cfg.en_protect_type = HAL_FCS_PROTECT_TYPE_NONE;
        }

        mac_fcs_start(pst_fcs_mgr, pst_fcs_cfg, 0, uc_fake_q_id);
        mac_fcs_release(pst_fcs_mgr);
    }
    else
#endif
    {
        /* 同频双STA模式，需要起两次one packet */
        if (WLAN_VAP_MODE_BSS_STA == pst_mac_vap1->en_vap_mode && WLAN_VAP_MODE_BSS_STA == pst_mac_vap2->en_vap_mode)
        {
            /* 准备VAP1的fcs参数 */
            pst_fcs_cfg->st_src_chl = pst_mac_vap1->st_channel;
            mac_fcs_prepare_one_packet_cfg(pst_mac_vap1, &(pst_fcs_cfg->st_one_packet_cfg), us_protect_time);

            /* 准备VAP2的fcs参数 */
            pst_fcs_cfg->st_src_chl2 = pst_mac_vap2->st_channel;
            mac_fcs_prepare_one_packet_cfg(pst_mac_vap2, &(pst_fcs_cfg->st_one_packet_cfg2), us_protect_time);
            pst_fcs_cfg->st_one_packet_cfg2.us_timeout = MAC_FCS_DEFAULT_PROTECT_TIME_OUT2;     /* 减小第二次one packet的保护时长，从而减少总时长 */

            mac_fcs_start_enhanced(pst_fcs_mgr, pst_fcs_cfg);
            mac_fcs_release(pst_fcs_mgr);
        }
        /* 同频STA+GO模式，只需要STA起一次one packet */
        else
        {
            if (WLAN_VAP_MODE_BSS_STA == pst_mac_vap1->en_vap_mode)
            {
                pst_vap_sta = pst_mac_vap1;
            }
            else
            {
                pst_vap_sta = pst_mac_vap2;
            }

            pst_fcs_cfg->st_src_chl = pst_vap_sta->st_channel;
            mac_fcs_prepare_one_packet_cfg(pst_vap_sta, &(pst_fcs_cfg->st_one_packet_cfg), us_protect_time);

            /* 调用FCS切信道接口 保存当前硬件队列的帧到扫描虚假队列 */
            mac_fcs_start(pst_fcs_mgr, pst_fcs_cfg, 0, HAL_TX_FAKE_QUEUE_BGSCAN_ID);
            mac_fcs_release(pst_fcs_mgr);
        }
    }

    return OAL_SUCC;
}



oal_void  dmac_scan_switch_channel_off(mac_device_stru *pst_mac_device)
{
    mac_vap_stru                   *pst_mac_vap;    /* 要发保护帧的VAP */
    wlan_scan_mode_enum_uint8       en_scan_mode;
    dmac_vap_stru                  *pst_dmac_vap;   /* 发起扫描的VAP */
    oal_uint8                       uc_up_vap_num;
    oal_uint32                      ul_ret;
    mac_vap_stru                   *pst_mac_vap2;

    en_scan_mode = pst_mac_device->st_scan_params.en_scan_mode;

    if (en_scan_mode >= WLAN_SCAN_MODE_BUTT)
    {
        OAM_ERROR_LOG1(0, OAM_SF_SCAN, "{dmac_scan_switch_channel_off::scan mode[%d] is invalid.}", en_scan_mode);
        return;
    }

    /* 如果是CCA算法优化发起的扫描，无需切信道 */
    if (WLAN_SCAN_MODE_BACKGROUND_CCA == en_scan_mode)
    {
        //OAM_WARNING_LOG0(0, OAM_SF_SCAN, "{dmac_scan_switch_channel_off::CCA ALG scan, don't need switch channel.}");
        return;
    }

    pst_dmac_vap = (dmac_vap_stru *)mac_res_get_dmac_vap(pst_mac_device->st_scan_params.uc_vap_id);
    if (OAL_PTR_NULL == pst_dmac_vap)
    {
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "{dmac_scan_switch_channel_off::pst_dmac_vap null.}");
        return;
    }

    /* 切信道之前 通知算法 */
    dmac_scan_switch_channel_notify_alg(pst_dmac_vap, &(pst_mac_device->st_scan_params.ast_channel_list[pst_mac_device->uc_scan_chan_idx]));

    /* 前景扫描或者PNO发起的扫描(注: PNO只在设备未关联的状态下才发起扫描) 直接切信道 */
#ifdef _PRE_WLAN_FEATURE_PROXYSTA
    if (!pst_mac_device->st_scan_params.en_need_switch_back_home_channel && ((WLAN_SCAN_MODE_FOREGROUND == en_scan_mode) || (WLAN_SCAN_MODE_BACKGROUND_PNO == en_scan_mode)))
#else
    if ((WLAN_SCAN_MODE_FOREGROUND == en_scan_mode) || (WLAN_SCAN_MODE_BACKGROUND_PNO == en_scan_mode))
#endif
    {
        /* 非关联状态下的扫描，保证1M dbb scaling与11M一致 */
    #if (_PRE_MULTI_CORE_MODE_OFFLOAD_DMAC == _PRE_MULTI_CORE_MODE)
    #ifdef _PRE_PLAT_FEATURE_CUSTOMIZE
        dmac_config_update_dsss_scaling_reg(DMAC_ALG_TPC_FAR_DISTANCE);
    #endif  /* _PRE_PLAT_FEATURE_CUSTOMIZE */
    #endif

    #ifdef _PRE_WLAN_FEATURE_PROXYSTA
        if (mac_is_proxysta_enabled(pst_mac_device) && mac_vap_is_vsta(&pst_dmac_vap->st_vap_base_info))
        {
            /* proxysta不切信道 */
            OAM_INFO_LOG0(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN, "{dmac_scan_switch_channel_off::proxysta DO NOT switch channnel.}");
            return;
        }
    #endif

        OAM_WARNING_LOG2(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_DBAC, "dmac_scan_switch_channel_off::scan mode[%d], switch channel to %d",
                        en_scan_mode,pst_mac_device->st_scan_params.ast_channel_list[pst_mac_device->uc_scan_chan_idx].uc_chan_number);
        /* 前景扫描或者PNO发起的扫描， 扫描切离home信道，不需要清除FIFO */
        dmac_mgmt_switch_channel(pst_mac_device, &pst_mac_device->st_scan_params.ast_channel_list[pst_mac_device->uc_scan_chan_idx], OAL_FALSE);
        return;
    }

    /****** 背景扫描，调用FCS接口切信道 ************/
    uc_up_vap_num = (oal_uint8)mac_device_calc_up_vap_num(pst_mac_device);

    if (1 == uc_up_vap_num)
    {
        if (OAL_SUCC != mac_device_find_up_vap(pst_mac_device, &pst_mac_vap))
        {
            OAM_ERROR_LOG0(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN, "{dmac_scan_switch_channel_off::up vap is null ptr.}");
            return;
        }

        /* 暂停此VAP发送 */
        dmac_vap_pause_tx(pst_mac_vap);

        /* 切离信道 */
        dmac_switch_channel_off(pst_mac_device,
                                pst_mac_vap,
                                &(pst_mac_device->st_scan_params.ast_channel_list[pst_mac_device->uc_scan_chan_idx]),
                                pst_mac_device->st_scan_params.us_scan_time);
    }
    else if (2 == uc_up_vap_num)
    {
        ul_ret = mac_device_find_2up_vap(pst_mac_device, &pst_mac_vap, &pst_mac_vap2);
        if (OAL_SUCC != ul_ret)
        {
            OAM_ERROR_LOG0(0, OAM_SF_SCAN, "mac_device_find_2up_vap return fail.");
            return;
        }

        dmac_switch_channel_off_enhanced(pst_mac_device,
                                         pst_mac_vap,
                                         pst_mac_vap2,
                                         &(pst_mac_device->st_scan_params.ast_channel_list[pst_mac_device->uc_scan_chan_idx]),
                                         pst_mac_device->st_scan_params.us_scan_time);
    }
    else if(uc_up_vap_num >= 3)
    {
        oal_uint8       uc_vap_idx;
        mac_vap_stru   *pst_vap;

        mac_device_find_up_vap(pst_mac_device, &pst_mac_vap);

        for (uc_vap_idx = 0; uc_vap_idx < pst_mac_device->uc_vap_num; uc_vap_idx++)
        {
            pst_vap = mac_res_get_mac_vap(pst_mac_device->auc_vap_id[uc_vap_idx]);

            if (MAC_VAP_STATE_UP == pst_vap->en_vap_state || MAC_VAP_STATE_PAUSE == pst_vap->en_vap_state)
            {
                dmac_vap_pause_tx(pst_vap);
            }
        }

        dmac_switch_channel_off(pst_mac_device,
                                pst_mac_vap,
                                &(pst_mac_device->st_scan_params.ast_channel_list[pst_mac_device->uc_scan_chan_idx]),
                                pst_mac_device->st_scan_params.us_scan_time);
    }
}


oal_void  dmac_scan_switch_channel_back(mac_device_stru *pst_mac_device)
{
    if (mac_is_dbac_running(pst_mac_device))
    {
        if (OAL_FALSE == pst_mac_device->st_scan_params.en_working_in_home_chan)
        {
            /* clear fifo when dbac resume*/
            dmac_tx_clear_tx_queue(pst_mac_device->pst_device_stru);
        }
        /* dbac场景只需恢复dbac，由dbac自行切到工作信道 */
        dmac_alg_dbac_resume(pst_mac_device);
        return;
    }

    if (OAL_FALSE == pst_mac_device->st_scan_params.en_working_in_home_chan)
    {
        /* 切回工作VAP所在的信道 */
        OAM_WARNING_LOG4(0, OAM_SF_SCAN, "{dmac_scan_switch_channel_back::switch home channel[%d], band[%d], bw[%d], restore tx queue from %d.}",
                      pst_mac_device->st_home_channel.uc_chan_number,
                      pst_mac_device->st_home_channel.en_band,
                      pst_mac_device->st_home_channel.en_bandwidth,
                      HAL_TX_FAKE_QUEUE_BGSCAN_ID);

        /* 扫描切回工作信道，当前信道的帧已经不再需要，清除FIFO */
        dmac_mgmt_switch_channel(pst_mac_device, &(pst_mac_device->st_home_channel), OAL_TRUE);

        /* 将扫描虚假队列里的帧恢复到硬件队列中去 */
        dmac_tx_restore_tx_queue(pst_mac_device->pst_device_stru, HAL_TX_FAKE_QUEUE_BGSCAN_ID);

        /* 恢复home信道上被暂停的发送 */
        dmac_vap_resume_tx_by_chl(pst_mac_device, &(pst_mac_device->st_home_channel));
    }

}


OAL_STATIC oal_bool_enum_uint8  dmac_scan_need_switch_home_channel(mac_device_stru *pst_mac_device)
{
    mac_scan_req_stru       *pst_scan_params;

    pst_scan_params = &(pst_mac_device->st_scan_params);

    /* 背景扫描需要切回工作信道 */
    /* en_need_switch_back_home_channel 必须最先判断 */
    if (OAL_TRUE == pst_mac_device->st_scan_params.en_need_switch_back_home_channel)
    {
        if (0 == pst_scan_params->uc_scan_channel_interval)
        {
            OAM_WARNING_LOG0(0, OAM_SF_SCAN, "{dmac_scan_need_switch_home_channel::scan_channel_interval is 0, set default value 6!}");
            pst_scan_params->uc_scan_channel_interval = MAC_SCAN_CHANNEL_INTERVAL_DEFAULT;
        }

        return (0 == pst_mac_device->uc_scan_chan_idx % pst_scan_params->uc_scan_channel_interval);
    }

    return OAL_FALSE;
}

#if defined(_PRE_PRODUCT_ID_HI110X_DEV)

OAL_STATIC oal_uint32  dmac_scan_check_2g_scan_results(mac_device_stru *pst_mac_device, mac_vap_stru *pst_vap, wlan_channel_band_enum_uint8 en_next_band)
{

    if (((WLAN_SCAN_MODE_FOREGROUND == pst_mac_device->st_scan_params.en_scan_mode)
         || (WLAN_SCAN_MODE_BACKGROUND_STA == pst_mac_device->st_scan_params.en_scan_mode))
         && (OAL_TRUE != pst_mac_device->st_scan_params.bit_is_p2p0_scan))
    {
        if (WLAN_BAND_2G == pst_mac_device->st_scan_params.uc_last_channel_band)
        {
            if (pst_mac_device->en_scan_curr_chan_find_bss_flag == OAL_TRUE)
            {
                pst_mac_device->uc_scan_ap_num_in_2p4++;
                pst_mac_device->en_scan_curr_chan_find_bss_flag = OAL_FALSE;
            }

            if ((WLAN_BAND_5G == en_next_band)
               || (pst_mac_device->uc_scan_chan_idx >= pst_mac_device->st_scan_params.uc_channel_nums))
            {
                if ((pst_mac_device->uc_scan_ap_num_in_2p4 <= 2) || ((pst_mac_device->uc_scan_count % 30) == 0))
                {
                    OAM_INFO_LOG2(0, OAM_SF_DBAC, "{dmac_scan_check_2g_scan_results::2.4G scan ap num = %d, scan_count = %d.}",
                                  pst_mac_device->uc_scan_ap_num_in_2p4,
                                  pst_mac_device->uc_scan_count);
#ifdef _PRE_WLAN_DFT_STAT
                    dmac_dft_report_all_ota_state(pst_vap);
#endif
                }
            }
        }
    }

    return OAL_SUCC;
}
#endif /* _PRE_PRODUCT_ID_HI110X_DEV */


OAL_STATIC oal_void dmac_scan_update_dfs_channel_scan_param(mac_device_stru     *pst_mac_device,
                                                            mac_channel_stru    *pst_mac_channel,
                                                            oal_uint16          *pus_scan_time,
                                                            oal_bool_enum_uint8 *pen_send_probe_req)
{
    mac_vap_stru        *pst_mac_vap;
    mac_scan_req_stru   *pst_scan_params = &(pst_mac_device->st_scan_params);

    /* 非雷达信道，需要发送probe req，扫描时间从扫描参数中获取 */
    if (OAL_FALSE == mac_is_dfs_channel(pst_mac_channel->en_band, pst_mac_channel->uc_chan_number))
    {
        *pen_send_probe_req = OAL_TRUE;
        *pus_scan_time      = pst_scan_params->us_scan_time;
        return;
    }

    /* 如果当前为关联状态，且关联AP 信道和本次扫描信道相同，则认为该信道无雷达，直接发送probe req 发起扫描 */
    pst_mac_vap = mac_res_get_mac_vap(pst_scan_params->uc_vap_id);
    if (OAL_PTR_NULL == pst_mac_vap)
    {
        OAM_WARNING_LOG1(pst_scan_params->uc_vap_id, OAM_SF_SCAN,
                        "{dmac_scan_update_dfs_channel_scan_param::get vap [%d] fail.}",
                        pst_scan_params->uc_vap_id);
        *pen_send_probe_req = OAL_FALSE;
        *pus_scan_time      = WLAN_DEFAULT_PASSIVE_SCAN_TIME;
        return;
    }
    if ((MAC_VAP_STATE_UP == pst_mac_vap->en_vap_state || MAC_VAP_STATE_PAUSE == pst_mac_vap->en_vap_state)
        && (pst_mac_channel->uc_chan_number == pst_mac_vap->st_channel.uc_chan_number))
    {
        *pen_send_probe_req = OAL_TRUE;
        *pus_scan_time      = pst_scan_params->us_scan_time;
        return;
    }

    /* 雷达信道第一次扫描，不发送probe req ，且在该信道暂停60ms
     * 雷达信道第二次扫描，如果在第一次扫描时候没有发现有AP，
     *                     则退出本信道扫描
     *                     如果在第一次扫描时候发现有AP，
     *                     则发送probe req ，且在该信道暂停20ms
     */
    if (pst_scan_params->uc_curr_channel_scan_count == 0)
    {
        /* 雷达信道第一次扫描，不发送probe req ，在该信道监听60ms */
        *pen_send_probe_req = OAL_FALSE;
        *pus_scan_time      = WLAN_DEFAULT_PASSIVE_SCAN_TIME;
    }
    else
    {
        if (pst_mac_device->en_scan_curr_chan_find_bss_flag == OAL_TRUE)
        {
            /* 雷达信道第二次扫描，且第一次扫描时有发现AP,
             * 设置需要发送probe req，扫描时间从扫描参数获取
             */
            *pen_send_probe_req = OAL_TRUE;
            *pus_scan_time      = pst_scan_params->us_scan_time;
        }
        else
        {
            /* 雷达信道第二次扫描，且第一次扫描时没有发现AP,
             * 设置超时定时器为0
             */
            *pen_send_probe_req = OAL_FALSE;
            *pus_scan_time      = 0;
        }
    }

    return;
}


OAL_STATIC oal_void dmac_scan_do_next_channel_scan(mac_device_stru  *pst_mac_device,
                                                   dmac_vap_stru    *pst_dmac_vap,
                                                   mac_channel_stru *pst_next_scan_channel)
{
#ifdef _PRE_WLAN_FEATURE_BTCOEX
    hal_to_dmac_device_stru *pst_hal_device = OAL_PTR_NULL;

    pst_hal_device = pst_mac_device->pst_device_stru;
    if (OAL_PTR_NULL == pst_hal_device)
    {
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "dmac_btcoex_scan_begin:pst_hal_device is null");
        return;
    }
#endif
#if defined(_PRE_PRODUCT_ID_HI110X_DEV)
    dmac_scan_check_2g_scan_results(pst_mac_device, &(pst_dmac_vap->st_vap_base_info), pst_next_scan_channel->en_band);
    pst_mac_device->st_scan_params.uc_last_channel_band = pst_next_scan_channel->en_band;
#endif
    pst_mac_device->en_scan_curr_chan_find_bss_flag = OAL_FALSE;       /* 本信道扫描到BSS 状态清除 */

    /* 切信道前通知算法 */
    dmac_scan_switch_channel_notify_alg(pst_dmac_vap, pst_next_scan_channel);

    //dmac_tx_clear_tx_queue(pst_mac_device->pst_device_stru);

    /* 切信道进行扫描 */ /* 切到下一个信道扫描，当前信道帧需要清除FIFO */
    dmac_mgmt_switch_channel(pst_mac_device, pst_next_scan_channel, OAL_TRUE);
#ifdef _PRE_WLAN_FEATURE_BTCOEX
    if(HAL_BTCOEX_SW_POWSAVE_WORK == GET_HAL_BTCOEX_SW_PREEMPT_TYPE(pst_hal_device))
    {
        GET_HAL_BTCOEX_SW_PREEMPT_TYPE(pst_hal_device) = HAL_BTCOEX_SW_POWSAVE_SCAN_BEGIN;
        OAM_WARNING_LOG0(0, OAM_SF_COEX, "{dmac_scan_do_next_channel_scan:: normal scan begin delay by btcoex!}");
    }
    else
#endif
    {
        dmac_scan_begin(pst_mac_device);
    }
}


OAL_STATIC oal_uint32  dmac_scan_curr_channel_scan_time_out(void *p_arg)
{
    mac_device_stru         *pst_mac_device = (mac_device_stru *)p_arg;
    mac_scan_req_stru       *pst_scan_params;
    mac_channel_stru        *pst_next_scan_channel = OAL_PTR_NULL;
    dmac_vap_stru           *pst_dmac_vap;
    hal_to_dmac_device_stru *pst_hal_device = OAL_PTR_NULL;
    oal_uint8                uc_do_meas;
#ifdef _PRE_WLAN_FEATURE_BTCOEX
    pst_hal_device = pst_mac_device->pst_device_stru;
    if (OAL_PTR_NULL == pst_hal_device)
    {
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "dmac_btcoex_scan_begin:pst_hal_device is null");
        return OAL_ERR_CODE_PTR_NULL;
    }
#endif
    /* 获取扫描参数 */
    pst_scan_params = &(pst_mac_device->st_scan_params);

    /* 更新本次信道的扫描次数 */
    pst_scan_params->uc_curr_channel_scan_count++;

    /* 根据扫描参数中信道扫描最大次数，判断是否切换信道号 */
    if (pst_scan_params->uc_curr_channel_scan_count >= pst_scan_params->uc_max_scan_count_per_channel)
    {
        /* 本信道扫描结束，如果下发的扫描参数需要上报信道统计信息，则上报，默认关闭 */
        uc_do_meas = pst_mac_device->st_scan_params.uc_scan_func & (MAC_SCAN_FUNC_MEAS | MAC_SCAN_FUNC_STATS);
        if (uc_do_meas)
        {
            /* 如果是CCA优化算法发起的扫描，无需上报信道测量结果 */
            if (WLAN_SCAN_MODE_BACKGROUND_CCA != pst_mac_device->st_scan_params.en_scan_mode)
            {
                /* 上报信道测量结果 */
                dmac_scan_report_channel_statistics_result(pst_mac_device, pst_mac_device->uc_scan_chan_idx);

                /* 清空信道测量结果，准备下一次信道测量值的统计 */
                OAL_MEMZERO(&(pst_mac_device->st_chan_result), OAL_SIZEOF(mac_scan_chan_stats_stru));
            }
        }

        pst_mac_device->uc_scan_chan_idx += 1;              /* 切换信道 */
        pst_scan_params->uc_curr_channel_scan_count = 0;    /* 本信道扫描次数清零 */
    }
    else
    {
#ifdef _PRE_WLAN_FEATURE_BTCOEX
        if(HAL_BTCOEX_SW_POWSAVE_WORK == GET_HAL_BTCOEX_SW_PREEMPT_TYPE(pst_hal_device))
        {
            GET_HAL_BTCOEX_SW_PREEMPT_TYPE(pst_hal_device) = HAL_BTCOEX_SW_POWSAVE_SCAN_BEGIN;
            OAM_WARNING_LOG0(0, OAM_SF_COEX, "{dmac_btcoex_scan_begin:: normal scan begin delay by btcoex!}");
        }
        else
#endif
        {
            /* 本信道扫描次数未完成，无需切换信道，直接发起扫描 */
            dmac_scan_begin(pst_mac_device);
        }
        return OAL_SUCC;
    }

    /* 获取发起扫描的dmac vap结构信息 */
    pst_dmac_vap = (dmac_vap_stru *)mac_res_get_dmac_vap(pst_mac_device->st_scan_params.uc_vap_id);
    if (OAL_PTR_NULL == pst_dmac_vap)
    {
        OAM_WARNING_LOG0(pst_mac_device->st_scan_params.uc_vap_id,OAM_SF_PWR,"{dmac_scan_curr_channel_scan_time_out::mac_res_get_dmac_vap fail}");
        return OAL_ERR_CODE_PTR_NULL;
    }

    /* 此次扫描请求完成，做一些收尾工作 */
    if (pst_mac_device->uc_scan_chan_idx >= pst_mac_device->st_scan_params.uc_channel_nums)
    {
#if defined(_PRE_PRODUCT_ID_HI110X_DEV)
        dmac_scan_check_2g_scan_results(pst_mac_device, &(pst_dmac_vap->st_vap_base_info), pst_mac_device->st_scan_params.uc_last_channel_band);
#endif
#ifdef _PRE_WLAN_FEATURE_BTCOEX
        if (HAL_BTCOEX_SW_POWSAVE_WORK == GET_HAL_BTCOEX_SW_PREEMPT_TYPE(pst_hal_device))
        {
            GET_HAL_BTCOEX_SW_PREEMPT_TYPE(pst_hal_device) = HAL_BTCOEX_SW_POWSAVE_SCAN_END;
        }
        else
#endif
        {
            dmac_scan_end(pst_mac_device);
        }
        return OAL_SUCC;
    }

    if ((WLAN_SCAN_MODE_FOREGROUND == pst_mac_device->st_scan_params.en_scan_mode) ||
            (WLAN_SCAN_MODE_BACKGROUND_PNO == pst_mac_device->st_scan_params.en_scan_mode))
    {
        /* 获取下一个要扫描的信道信息 */
        pst_next_scan_channel = &(pst_mac_device->st_scan_params.ast_channel_list[pst_mac_device->uc_scan_chan_idx]);
        dmac_scan_do_next_channel_scan(pst_mac_device, pst_dmac_vap, pst_next_scan_channel);
    }
    else
    {
        /* 背景扫描，判断是否需要返回工作信道工作一段时间，如果是，则切回工作信道工作 */
        if (OAL_TRUE == dmac_scan_need_switch_home_channel(pst_mac_device))
        {
#ifdef _PRE_WLAN_FEATURE_BTCOEX
            if (HAL_BTCOEX_SW_POWSAVE_WORK == GET_HAL_BTCOEX_SW_PREEMPT_TYPE(pst_hal_device))
            {
                GET_HAL_BTCOEX_SW_PREEMPT_TYPE(pst_hal_device) = HAL_BTCOEX_SW_POWSAVE_SCAN_WAIT;
            }
            else
#endif
            {
                dmac_scan_switch_home_channel_work(pst_mac_device);
            }
            pst_mac_device->st_scan_params.en_working_in_home_chan = OAL_TRUE;
        }
        else
        {
            pst_mac_device->st_scan_params.en_working_in_home_chan = OAL_FALSE;
            /* 获取下一个要扫描的信道信息 */
            pst_next_scan_channel = &(pst_mac_device->st_scan_params.ast_channel_list[pst_mac_device->uc_scan_chan_idx]);
            dmac_scan_do_next_channel_scan(pst_mac_device, pst_dmac_vap, pst_next_scan_channel);
        }
    }

    return OAL_SUCC;
}


OAL_STATIC oal_void dmac_pno_scan_send_probe_with_ssid(mac_device_stru *pst_mac_device, oal_uint8 uc_band)
{
    mac_pno_sched_scan_mgmt_stru *pst_pno_sched_scan_mgmt;
    dmac_vap_stru                *pst_dmac_vap;
    oal_uint8                     uc_band_tmp;
    oal_uint8                     uc_loop;
    oal_uint32                    ul_ret;

    if(OAL_PTR_NULL == pst_mac_device)
    {
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "{dmac_pno_scan_send_probe_with_ssid::pst_mac_device is null.}");
        return;
    }

    pst_pno_sched_scan_mgmt = pst_mac_device->pst_pno_sched_scan_mgmt;

    pst_dmac_vap = (dmac_vap_stru *)mac_res_get_dmac_vap(pst_mac_device->st_scan_params.uc_vap_id);
    if (OAL_PTR_NULL == pst_dmac_vap)
    {
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "{dmac_pno_scan_send_probe_with_ssid::pst_dmac_vap null.}");
        return;
    }

    
    if(OAL_PTR_NULL == pst_dmac_vap->st_vap_base_info.pst_mib_info)
    {
        OAM_ERROR_LOG4(0, OAM_SF_SCAN, "{dmac_pno_scan_send_probe_with_ssid:: vap mib info is null,uc_vap_id[%d], p_fn_cb[%p], uc_scan_func[%d], uc_curr_channel_scan_count[%d].}",
                   pst_mac_device->st_scan_params.uc_vap_id,
                   pst_mac_device->st_scan_params.p_fn_cb,
                   pst_mac_device->st_scan_params.uc_scan_func,
                   pst_mac_device->st_scan_params.uc_curr_channel_scan_count);
        return;
    }


    /* 发探测请求时，需要临时更新vap的band信息，防止5G发11b */
    uc_band_tmp = pst_dmac_vap->st_vap_base_info.st_channel.en_band;

    pst_dmac_vap->st_vap_base_info.st_channel.en_band = uc_band;

    for(uc_loop = 0; uc_loop < pst_pno_sched_scan_mgmt->st_pno_sched_scan_params.l_ssid_count; uc_loop++)
    {
        if(OAL_TRUE == pst_pno_sched_scan_mgmt->st_pno_sched_scan_params.ast_match_ssid_set[uc_loop].en_scan_ssid)
        {
            /* 如果是隐藏SSID,则指定SSID扫描 */
            ul_ret = dmac_scan_send_probe_req_frame(pst_dmac_vap, BROADCAST_MACADDR, (oal_int8 *)pst_pno_sched_scan_mgmt->st_pno_sched_scan_params.ast_match_ssid_set[uc_loop].auc_ssid);
            if (OAL_SUCC != ul_ret)
            {
                OAM_WARNING_LOG1(0, OAM_SF_SCAN, "{dmac_pno_scan_send_probe_with_ssid::dmac_scan_send_probe_req_frame failed[%u].}", ul_ret);
            }
        }
    }

    pst_dmac_vap->st_vap_base_info.st_channel.en_band = uc_band_tmp;
    return;
}


oal_void dmac_scan_begin(mac_device_stru *pst_mac_device)
{
    mac_scan_req_stru               *pst_scan_params;
    dmac_vap_stru                   *pst_dmac_vap;
    oal_uint32                       ul_ret;
    oal_uint8                        uc_band;
    oal_uint8                        uc_do_bss_scan;
    oal_uint8                        uc_do_meas;
    oal_uint8                        uc_loop;
    oal_uint8                        uc_do_p2p_listen = 0;
    oal_bool_enum_uint8              en_send_probe_req;
    oal_uint16                       us_scan_time;

    /* 获取扫描参数 */
    pst_scan_params = &(pst_mac_device->st_scan_params);

    uc_band          = pst_scan_params->ast_channel_list[pst_mac_device->uc_scan_chan_idx].en_band;
    uc_do_bss_scan   = pst_scan_params->uc_scan_func & MAC_SCAN_FUNC_BSS;
    uc_do_p2p_listen = pst_scan_params->uc_scan_func & MAC_SCAN_FUNC_P2P_LISTEN;

    /* 如果当前扫描模式需要统计信道信息，则使能对应寄存器 */
    uc_do_meas = pst_scan_params->uc_scan_func & (MAC_SCAN_FUNC_MEAS | MAC_SCAN_FUNC_STATS);
    if (uc_do_meas)
    {
        /* 使能信道测量中断 */
        hal_set_ch_statics_period(pst_mac_device->pst_device_stru, DMAC_SCAN_CHANNEL_STATICS_PERIOD_US);
        hal_set_ch_measurement_period(pst_mac_device->pst_device_stru, DMAC_SCAN_CHANNEL_MEAS_PERIOD_MS);
        hal_enable_ch_statics(pst_mac_device->pst_device_stru, 1);
    }
    pst_dmac_vap = mac_res_get_dmac_vap(pst_scan_params->uc_vap_id);

#ifdef _PRE_WLAN_FEATURE_11K
    if(OAL_TRUE == pst_dmac_vap->bit_11k_enable)
    {
        if (WLAN_SCAN_MODE_RRM_BEACON_REQ == pst_scan_params->en_scan_mode)
        {
            hal_vap_tsf_get_64bit(pst_dmac_vap->pst_hal_vap, (oal_uint32 *)&(pst_dmac_vap->pst_rrm_info->auc_act_meas_start_time[4]), (oal_uint32 *)&(pst_dmac_vap->pst_rrm_info->auc_act_meas_start_time[0]));
            OAM_WARNING_LOG1(0, OAM_SF_SCAN, "{dmac_scan_begin::update start tsf ok, vap id[%d].}", pst_scan_params->uc_vap_id);
        }
    }
#endif

    /* dfs信道判断，如果是雷达信道，执行被动扫描 */
    dmac_scan_update_dfs_channel_scan_param(pst_mac_device,
                                            &(pst_scan_params->ast_channel_list[pst_mac_device->uc_scan_chan_idx]),
                                            &us_scan_time,
                                            &en_send_probe_req);

    /* ACTIVE方式下发送广播RPOBE REQ帧 */
    if (uc_do_bss_scan && (WLAN_SCAN_TYPE_ACTIVE == pst_scan_params->en_scan_type)
        && (OAL_TRUE == en_send_probe_req))
    {
        /* PNO指定SSID扫描,最多指定16个SSID */
        if(WLAN_SCAN_MODE_BACKGROUND_PNO == pst_mac_device->st_scan_params.en_scan_mode)
        {
            dmac_pno_scan_send_probe_with_ssid(pst_mac_device, uc_band);
        }

        /* 每次信道发送的probe req帧的个数 */
        for (uc_loop = 0; uc_loop < pst_scan_params->uc_max_send_probe_req_count_per_channel; uc_loop++)
        {
            ul_ret = dmac_scan_send_bcast_probe(pst_mac_device, uc_band, uc_loop);
            if (OAL_SUCC != ul_ret)
            {
                OAM_WARNING_LOG1(0, OAM_SF_SCAN, "{dmac_scan_begin::dmac_scan_send_bcast_probe failed[%d].}", ul_ret);
            }
        }
    }
#ifdef _PRE_WLAN_FEATURE_11K
#if (_PRE_PRODUCT_ID == _PRE_PRODUCT_ID_HI1102_DEV)
     if(OAL_TRUE == pst_dmac_vap->bit_11k_enable)
     {
         if (WLAN_SCAN_MODE_RRM_BEACON_REQ == pst_scan_params->en_scan_mode
            && WLAN_SCAN_TYPE_PASSIVE == pst_scan_params->en_scan_type)
         {
             hal_disable_non_frame_mgmt_filter(pst_mac_device->pst_device_stru);
         }
     }
#endif
#endif
    /* 放在发送完成所有probe request报文再启动定时器,防止指定SSID扫描报文过多,定时器时间内都在发送,用于接收扫描结果时间过短 */
    /* 启动扫描定时器 */
    FRW_TIMER_CREATE_TIMER(&pst_mac_device->st_scan_timer,
                           dmac_scan_curr_channel_scan_time_out,
                           us_scan_time,
                           pst_mac_device,
                           OAL_FALSE,
                           OAM_MODULE_ID_DMAC,
                           pst_mac_device->ul_core_id);

    /* p2p监听处理逻辑 */
    /* p2p listen时需要更改VAP的信道，组probe rsp帧(DSSS ie, ht ie)需要。listen结束后恢复 */
    if (uc_do_p2p_listen)
    {
        pst_mac_device->st_p2p_vap_channel = pst_dmac_vap->st_vap_base_info.st_channel;

        pst_dmac_vap->st_vap_base_info.st_channel = pst_scan_params->ast_channel_list[0];

#ifdef _PRE_WLAN_FEATURE_STA_PM
        /* 修改p2p ci 问题。P2P 监听状态下，打开前端RF接收。 */
        dmac_pm_enable_front_end(pst_mac_device, OAL_TRUE);
#endif
    }

#ifdef _PRE_WLAN_FEATURE_BTCOEX
    if (!IS_LEGACY_VAP(&(pst_dmac_vap->st_vap_base_info)))
    {
        /* Notify Bt the P2P Scan Start state */
        hal_set_btcoex_soc_gpreg0(OAL_TRUE, BIT14, 14);
        hal_coex_sw_irq_set(HAL_COEX_SW_IRQ_BT);
    }
#endif
    return;
}


oal_void dmac_scan_end(mac_device_stru *pst_mac_device)
{
    dmac_vap_stru              *pst_dmac_vap;
    wlan_scan_mode_enum_uint8   en_scan_mode = WLAN_SCAN_MODE_BUTT;
    oal_uint8                   uc_do_p2p_listen;

    /* 获取dmac vap */
    pst_dmac_vap = mac_res_get_dmac_vap(pst_mac_device->st_scan_params.uc_vap_id);
    if (OAL_PTR_NULL == pst_dmac_vap)
    {
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "{dmac_scan_end::pst_dmac_vap is null.}");

        /* 恢复device扫描状态为空闲状态 */
        pst_mac_device->en_curr_scan_state = MAC_SCAN_STATE_IDLE;
        return;
    }

    /* listen时修改vap信道为listen信道，listen结束后需要恢复 */
    uc_do_p2p_listen = pst_mac_device->st_scan_params.uc_scan_func & MAC_SCAN_FUNC_P2P_LISTEN;
    if (uc_do_p2p_listen)
    {
        pst_dmac_vap->st_vap_base_info.st_channel = pst_mac_device->st_p2p_vap_channel;
    }

    /* 扫描结束时，根据是否为随机mac addr扫描，恢复vap原先的mac addr */
    dmac_scan_set_vap_mac_addr_by_scan_state(pst_mac_device, OAL_FALSE);

    /* 获取扫描模式 */
    en_scan_mode = pst_mac_device->st_scan_params.en_scan_mode;

    /* 如果是背景扫描，且不是cca发起的扫描，则无需切回工作信道 */
#ifdef _PRE_WLAN_FEATURE_PROXYSTA
    if (((en_scan_mode < WLAN_SCAN_MODE_BUTT) && (en_scan_mode != WLAN_SCAN_MODE_BACKGROUND_CCA)
           && (en_scan_mode != WLAN_SCAN_MODE_BACKGROUND_PNO))
           && (mac_is_proxysta_enabled(pst_mac_device)
           && (pst_mac_device->st_scan_params.uc_channel_nums != 1
           || pst_mac_device->st_scan_params.ast_channel_list[0].uc_chan_number != pst_mac_device->pst_device_stru->uc_current_chan_number)))
#else
    if (((en_scan_mode < WLAN_SCAN_MODE_BUTT) && (en_scan_mode != WLAN_SCAN_MODE_FOREGROUND) && (en_scan_mode != WLAN_SCAN_MODE_BACKGROUND_CCA)
           && (en_scan_mode != WLAN_SCAN_MODE_BACKGROUND_PNO)))
#endif
    {
        dmac_scan_switch_channel_back(pst_mac_device);
        dmac_scan_switch_channel_notify_alg(pst_dmac_vap, &(pst_mac_device->st_home_channel));
    }
    else
    {
        OAM_INFO_LOG0(0, OAM_SF_SCAN, "{dmac_scan_end::switch back not need.}");
    }

    /* 根据扫描模式进行对应扫描结果的处理 */
    switch (en_scan_mode)
    {
        case WLAN_SCAN_MODE_FOREGROUND:
        case WLAN_SCAN_MODE_BACKGROUND_STA:
        case WLAN_SCAN_MODE_BACKGROUND_AP:
        {
#ifdef _PRE_WLAN_FEATURE_STA_PM
            /* 平台的计数*/
            PM_WLAN_DumpSleepCnt();
#endif
            /* 上报扫描完成事件，扫描状态为成功 */
            (oal_void)dmac_scan_proc_scan_complete_event(pst_dmac_vap, MAC_SCAN_SUCCESS);
            break;
        }
        case WLAN_SCAN_MODE_BACKGROUND_OBSS:
        {
#if (_PRE_MULTI_CORE_MODE_OFFLOAD_DMAC == _PRE_MULTI_CORE_MODE)
#ifdef _PRE_WLAN_FEATURE_20_40_80_COEXIST
            dmac_scan_proc_obss_scan_complete_event(pst_dmac_vap);
#endif
#endif
            break;
        }
        case WLAN_SCAN_MODE_BACKGROUND_CCA:
        {
            /* CCA算法优化发起的扫描，在device侧发起，直接调用对应回调函数进行处理 */
            if (OAL_PTR_NULL != pst_mac_device->st_scan_params.p_fn_cb)
            {
                pst_mac_device->st_scan_params.p_fn_cb(pst_mac_device);
            }
            break;
        }
        case WLAN_SCAN_MODE_BACKGROUND_PNO:
        {
            /* 恢复原先的帧过滤寄存器值 */
            hal_set_rx_filter(pst_mac_device->pst_device_stru, 0x37B9EEFA);

            /* 是否扫描到了匹配的ssid，如果是，上报扫描结果; 否则进入睡眠 */
            if (OAL_TRUE == pst_mac_device->pst_pno_sched_scan_mgmt->en_is_found_match_ssid)
            {
                /* 上报扫描完成事件，扫描状态为成功 */
                (oal_void)dmac_scan_proc_scan_complete_event(pst_dmac_vap, MAC_SCAN_PNO);

                /* 释放PNO管理结构体内存 */
                OAL_MEM_FREE(pst_mac_device->pst_pno_sched_scan_mgmt, OAL_TRUE);
                pst_mac_device->pst_pno_sched_scan_mgmt = OAL_PTR_NULL;
            }

            /* 不管扫到扫不到,每次扫描完device都要投票睡眠 */
        #ifdef _PRE_WLAN_FEATURE_STA_PM
            hal_disable_sta_tsf_tbtt(pst_dmac_vap->pst_hal_vap);
            PM_WLAN_PsmHandle(PM_STA0_ID, PM_WLAN_SHUTDOWN_PROCESS);
        #endif

            break;
        }
        case WLAN_SCAN_MODE_BACKGROUND_CSA:
        {
            g_csa_scan_flag = OAL_FALSE;
            OAM_WARNING_LOG0(0, OAM_SF_SCAN, "{dmac_scan_end::scan_mode BACKGROUND_CSA}");
            break;
        }
#ifdef _PRE_WLAN_FEATURE_11K
        case WLAN_SCAN_MODE_RRM_BEACON_REQ:
        {
            /* 扫描结束后组响应帧，填充并发出 */
            /* 申请管理帧内存并填充头部信息 */
            dmac_rrm_encap_and_send_bcn_rpt(pst_dmac_vap);

#if (_PRE_PRODUCT_ID == _PRE_PRODUCT_ID_HI1102_DEV)
            hal_enable_non_frame_filter(pst_mac_device->pst_device_stru);
#endif
            break;
        }
#endif
        default:
        {
            OAM_ERROR_LOG1(0, OAM_SF_SCAN, "{dmac_scan_end::scan_mode[%d] error.}", en_scan_mode);
            break;
        }
    }

#ifdef _PRE_WLAN_FEATURE_BTCOEX
    /* CCA不需通知 */
    if (WLAN_SCAN_MODE_BACKGROUND_CCA != en_scan_mode)
    {
        hal_set_btcoex_soc_gpreg1(OAL_FALSE, BIT1, 1);   // 入网流程结束
        hal_set_btcoex_soc_gpreg1(OAL_FALSE, BIT0, 0);   // Wifi扫描结束，清除第一次上电扫描的状态
        hal_coex_sw_irq_set(HAL_COEX_SW_IRQ_BT);
    }
#endif

    /* 恢复device扫描状态为空闲状态 */
    pst_mac_device->en_curr_scan_state = MAC_SCAN_STATE_IDLE;
    pst_mac_device->st_scan_params.en_working_in_home_chan = OAL_FALSE;

    if((OAL_TRUE == pst_dmac_vap->st_vap_base_info.st_ch_switch_info.en_waiting_to_shift_channel)
       && (0 == pst_dmac_vap->st_vap_base_info.st_ch_switch_info.uc_new_ch_swt_cnt))
    {
        /* 将在下一次tbtt中断dmac_handle_tbtt_chan_mgmt_sta函数中进行信道切换 */
        pst_dmac_vap->st_vap_base_info.st_ch_switch_info.en_channel_swt_cnt_zero = OAL_TRUE;
    }

#ifdef _PRE_WLAN_FEATURE_BTCOEX
    if (!IS_LEGACY_VAP(&(pst_dmac_vap->st_vap_base_info)))
    {
        /* Notify Bt the P2P Scan End state */
        hal_set_btcoex_soc_gpreg0(OAL_FALSE, BIT14, 14);
        hal_coex_sw_irq_set(HAL_COEX_SW_IRQ_BT);
    }
#endif
    return;
}


oal_uint32  dmac_switch_same_channel_off(
                mac_device_stru     *pst_mac_device,
                mac_vap_stru        *pst_mac_vap,
                oal_uint16           us_protect_time)
{
    mac_fcs_mgr_stru         *pst_fcs_mgr;
    mac_fcs_cfg_stru         *pst_fcs_cfg;
#ifdef _PRE_WLAN_FEATURE_STA_PM
    mac_sta_pm_handler_stru  *pst_mac_sta_pm_handle;
#endif
    dmac_vap_stru            *pst_dmac_vap;

    if ((OAL_PTR_NULL == pst_mac_device) || (OAL_PTR_NULL == pst_mac_vap))
    {
        OAM_ERROR_LOG2(0, OAM_SF_SCAN, "dmac_switch_same_channel_off:pst_mac_device=%x,pst_mac_vap=%x",pst_mac_device,pst_mac_vap);
        return OAL_ERR_CODE_PTR_NULL;
    }
    /* 暂停vap业务 */
    dmac_vap_pause_tx(pst_mac_vap);

    pst_mac_device->st_home_channel = pst_mac_vap->st_channel;

    if (WLAN_VAP_MODE_BSS_STA == pst_mac_vap->en_vap_mode)
    {
        pst_dmac_vap = (dmac_vap_stru *)pst_mac_vap;
#ifdef _PRE_WLAN_FEATURE_STA_PM
        pst_mac_sta_pm_handle = (mac_sta_pm_handler_stru *)(pst_dmac_vap->pst_pm_handler);
        if (STA_PWR_SAVE_STATE_ACTIVE != STA_GET_PM_STATE(pst_mac_sta_pm_handle))
        {
            OAM_WARNING_LOG1(0,0,"dmac_switch_same_channel_off:pm state=%d",STA_GET_PM_STATE(pst_mac_sta_pm_handle));
            return OAL_SUCC;
        }
#endif
    }

    pst_fcs_mgr = dmac_fcs_get_mgr_stru(pst_mac_device);
    pst_fcs_cfg = &(pst_mac_device->st_fcs_cfg);

    OAL_MEMZERO(pst_fcs_cfg, OAL_SIZEOF(mac_fcs_cfg_stru));

    pst_fcs_cfg->st_src_chl = pst_mac_vap->st_channel;
    pst_fcs_cfg->st_dst_chl = pst_mac_vap->st_channel;

    mac_fcs_prepare_one_packet_cfg(pst_mac_vap, &(pst_fcs_cfg->st_one_packet_cfg), us_protect_time);

    /* 调用FCS切信道接口 保存当前硬件队列的帧到扫描虚假队列 */
    mac_fcs_start_same_channel(pst_fcs_mgr, pst_fcs_cfg, 0, HAL_TX_FAKE_QUEUE_BGSCAN_ID);
    mac_fcs_release(pst_fcs_mgr);
    return OAL_SUCC;
}


oal_uint32  dmac_switch_channel_off_enhanced_self_channel(
                mac_device_stru     *pst_mac_device,
                mac_vap_stru        *pst_mac_vap1,
                mac_vap_stru        *pst_mac_vap2,
                oal_uint16           us_protect_time)
{
    mac_fcs_mgr_stru               *pst_fcs_mgr;
    mac_fcs_cfg_stru               *pst_fcs_cfg;
    mac_vap_stru                   *pst_vap_sta;

    if ((OAL_PTR_NULL == pst_mac_device) || (OAL_PTR_NULL == pst_mac_vap1) || (OAL_PTR_NULL == pst_mac_vap2))
    {
        OAM_ERROR_LOG3(0, OAM_SF_SCAN, "dmac_switch_channel_off_enhanced_self_channel:pst_mac_device=%x,pst_mac_vap1=%x,pst_mac_vap2=%x"
                       ,pst_mac_device,pst_mac_vap1,pst_mac_vap2);
        return OAL_ERR_CODE_PTR_NULL;
    }
    /* 记录切离时最大带宽的信道，供同频共存扫描完后切回 */
    if (pst_mac_vap1->st_channel.en_bandwidth >= pst_mac_vap2->st_channel.en_bandwidth)
    {
        pst_mac_device->st_home_channel = pst_mac_vap1->st_channel;
    }
    else
    {
        pst_mac_device->st_home_channel = pst_mac_vap2->st_channel;
    }

    /* 暂停两个VAP的发送 */
    dmac_vap_pause_tx(pst_mac_vap1);
    dmac_vap_pause_tx(pst_mac_vap2);

    pst_fcs_mgr = dmac_fcs_get_mgr_stru(pst_mac_device);
    pst_fcs_cfg = &(pst_mac_device->st_fcs_cfg);
    OAL_MEMZERO(pst_fcs_cfg, OAL_SIZEOF(mac_fcs_cfg_stru));

    OAM_WARNING_LOG2(0, OAM_SF_SCAN, "{dmac_switch_channel_off_enhanced::curr hal chan[%d], dst channel[%d].}",
                  pst_mac_device->pst_device_stru->uc_current_chan_number,
                  pst_fcs_cfg->st_dst_chl.uc_chan_number);

    /* 同频双STA模式，需要起两次one packet */
    if (WLAN_VAP_MODE_BSS_STA == pst_mac_vap1->en_vap_mode && WLAN_VAP_MODE_BSS_STA == pst_mac_vap2->en_vap_mode)
    {
        /* 准备VAP1的fcs参数 */
        pst_fcs_cfg->st_src_chl = pst_mac_vap1->st_channel;
        pst_fcs_cfg->st_dst_chl = pst_mac_vap1->st_channel;
        mac_fcs_prepare_one_packet_cfg(pst_mac_vap1, &(pst_fcs_cfg->st_one_packet_cfg), us_protect_time);

        /* 准备VAP2的fcs参数 */
        pst_fcs_cfg->st_src_chl2 = pst_mac_vap2->st_channel;
        mac_fcs_prepare_one_packet_cfg(pst_mac_vap2, &(pst_fcs_cfg->st_one_packet_cfg2), us_protect_time);
        pst_fcs_cfg->st_one_packet_cfg2.us_timeout = MAC_FCS_DEFAULT_PROTECT_TIME_OUT2;     /* 减小第二次one packet的保护时长，从而减少总时长 */

        mac_fcs_start_enhanced_same_channel(pst_fcs_mgr, pst_fcs_cfg);
        mac_fcs_release(pst_fcs_mgr);
    }
    /* 同频STA+GO模式，只需要STA起一次one packet */
    else
    {
        if (WLAN_VAP_MODE_BSS_STA == pst_mac_vap1->en_vap_mode)
        {
            pst_vap_sta = pst_mac_vap1;
        }
        else
        {
            pst_vap_sta = pst_mac_vap2;
        }

        pst_fcs_cfg->st_src_chl = pst_vap_sta->st_channel;
        pst_fcs_cfg->st_dst_chl = pst_vap_sta->st_channel;
        mac_fcs_prepare_one_packet_cfg(pst_vap_sta, &(pst_fcs_cfg->st_one_packet_cfg), us_protect_time);

        /* 调用FCS切信道接口 保存当前硬件队列的帧到扫描虚假队列 */
        mac_fcs_start_same_channel(pst_fcs_mgr, pst_fcs_cfg, 0, HAL_TX_FAKE_QUEUE_BGSCAN_ID);
        mac_fcs_release(pst_fcs_mgr);
    }

    return OAL_SUCC;
}


oal_void  dmac_scan_abort(mac_device_stru *pst_mac_device)
{
    if (MAC_SCAN_STATE_RUNNING != pst_mac_device->en_curr_scan_state)
    {
        return;
    }

    /* 删除扫描定时器 */
    if (OAL_TRUE == pst_mac_device->st_scan_timer.en_is_registerd)
    {
        FRW_TIMER_IMMEDIATE_DESTROY_TIMER(&pst_mac_device->st_scan_timer);
    }

    pst_mac_device->st_scan_params.en_abort_scan_flag = OAL_TRUE;
    dmac_scan_end(pst_mac_device);
    pst_mac_device->st_scan_params.en_abort_scan_flag = OAL_FALSE;

    OAM_WARNING_LOG0(0, OAM_SF_SCAN, "dmac_scan_abort: scan has been aborted");
}


OAL_STATIC oal_uint32 dmac_scan_get_ssid_ie_info(mac_device_stru *pst_mac_device, oal_int8 *pc_ssid, oal_uint8  uc_index)
{
    dmac_vap_stru     *pst_dmac_vap;

    pst_dmac_vap = (dmac_vap_stru *)mac_res_get_dmac_vap(pst_mac_device->st_scan_params.uc_vap_id);

    if (IS_LEGACY_VAP(&(pst_dmac_vap->st_vap_base_info)))
    {
        /* 根据索引号，准备probe req帧的ssid信息 */
        oal_memcopy(pc_ssid, pst_mac_device->st_scan_params.ast_mac_ssid_set[uc_index].auc_ssid, WLAN_SSID_MAX_LEN);
    }
    else
    {
        /* P2P 设备扫描，需要获取指定ssid 信息，对P2P 设备，扫描时只扫描一个指定ssid */
        oal_memcopy(pc_ssid, pst_mac_device->st_scan_params.ast_mac_ssid_set[0].auc_ssid, WLAN_SSID_MAX_LEN);
    }

    return OAL_SUCC;
}



OAL_STATIC oal_uint32 dmac_scan_send_bcast_probe(mac_device_stru *pst_mac_device, oal_uint8 uc_band, oal_uint8  uc_index)
{
    oal_int8           ac_ssid[WLAN_SSID_MAX_LEN] = {'\0'};
    dmac_vap_stru     *pst_dmac_vap;
    oal_uint32         ul_ret;
    oal_uint8          uc_band_tmp;

    if (0 == pst_mac_device->uc_vap_num)
    {
        OAM_WARNING_LOG0(0, OAM_SF_SCAN, "{dmac_scan_send_bcast_probe::uc_vap_num=0.}");
        return OAL_FAIL;
    }

    pst_dmac_vap = (dmac_vap_stru *)mac_res_get_dmac_vap(pst_mac_device->st_scan_params.uc_vap_id);
    if (OAL_PTR_NULL == pst_dmac_vap)
    {
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "{dmac_scan_send_bcast_probe::pst_dmac_vap null.}");
        return OAL_ERR_CODE_PTR_NULL;
    }

    
    if(OAL_PTR_NULL == pst_dmac_vap->st_vap_base_info.pst_mib_info)
    {
        OAM_ERROR_LOG4(0, OAM_SF_SCAN, "{dmac_scan_send_bcast_probe:: vap mib info is null,uc_vap_id[%d], p_fn_cb[%p], uc_scan_func[%d], uc_curr_channel_scan_count[%d].}",
                   pst_mac_device->st_scan_params.uc_vap_id,
                   pst_mac_device->st_scan_params.p_fn_cb,
                   pst_mac_device->st_scan_params.uc_scan_func,
                   pst_mac_device->st_scan_params.uc_curr_channel_scan_count);
        return OAL_ERR_CODE_PTR_NULL;
    }

    /* 发探测请求时，需要临时更新vap的band信息，防止5G发11b */
    uc_band_tmp = pst_dmac_vap->st_vap_base_info.st_channel.en_band;

    pst_dmac_vap->st_vap_base_info.st_channel.en_band = uc_band;

    /* 获取本次扫描请求帧中需要携带的ssid ie信息 */
    ul_ret = dmac_scan_get_ssid_ie_info(pst_mac_device, ac_ssid, uc_index);
    if (OAL_SUCC != ul_ret)
    {
        OAM_ERROR_LOG1(0, OAM_SF_SCAN, "{dmac_scan_send_bcast_probe::get ssid failed, error[%d].}", ul_ret);
        return ul_ret;
    }

    /* 发送probe req帧 */
    ul_ret = dmac_scan_send_probe_req_frame(pst_dmac_vap, BROADCAST_MACADDR, ac_ssid);

    pst_dmac_vap->st_vap_base_info.st_channel.en_band = uc_band_tmp;

    if (OAL_SUCC != ul_ret)
    {
        OAM_WARNING_LOG1(pst_dmac_vap->st_vap_base_info.uc_vap_id, OAM_SF_SCAN, "{dmac_scan_send_bcast_probe::dmac_mgmt_send_probe_request failed[%d].}", ul_ret);
    }

    return ul_ret;
}


oal_void  dmac_scan_switch_home_channel_work(mac_device_stru *pst_mac_device)
{
    mac_scan_req_stru       *pst_scan_params;

    pst_scan_params = &(pst_mac_device->st_scan_params);

    hal_enable_radar_det(pst_mac_device->pst_device_stru, 0);

    /* 切回工作信道工作时，根据是否为随机mac addr扫描，恢复vap原先的mac addr */
    dmac_scan_set_vap_mac_addr_by_scan_state(pst_mac_device, OAL_FALSE);

    /* 背景扫描 切回工作信道 */
    dmac_scan_switch_channel_back(pst_mac_device);

    /* 判零检查 */
    if (0 == pst_scan_params->us_work_time_on_home_channel)
    {
        OAM_WARNING_LOG0(0, OAM_SF_SCAN, "{dmac_scan_switch_home_channel_work:work_time_on_home_channel is 0, set it to default 100ms!}");
        pst_scan_params->us_work_time_on_home_channel = MAC_WORK_TIME_ON_HOME_CHANNEL_DEFAULT;
    }

    /* 启动间隔定时器，在工作信道工作一段时间后，切回扫描信道进行扫描 */
    FRW_TIMER_CREATE_TIMER(&pst_mac_device->st_scan_timer,
                           dmac_scan_switch_home_channel_work_timeout,
                           pst_scan_params->us_work_time_on_home_channel,
                           pst_mac_device,
                           OAL_FALSE,
                           OAM_MODULE_ID_DMAC,
                           pst_mac_device->ul_core_id);

    return;
}


OAL_STATIC oal_uint32  dmac_scan_switch_home_channel_work_timeout(void *p_arg)
{
    mac_device_stru         *pst_mac_device;
#ifdef _PRE_WLAN_FEATURE_BTCOEX
    hal_to_dmac_device_stru *pst_hal_device = OAL_PTR_NULL;
#endif
    pst_mac_device = (mac_device_stru *)p_arg;
    if (OAL_PTR_NULL == pst_mac_device)
    {
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "{dmac_scan_switch_home_channel_work_timeout::pst_mac_device null.}");
        return OAL_ERR_CODE_PTR_NULL;
    }
#ifdef _PRE_WLAN_FEATURE_BTCOEX
    pst_hal_device = pst_mac_device->pst_device_stru;
    if (OAL_PTR_NULL == pst_hal_device)
    {
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "dmac_btcoex_scan_begin:pst_hal_device is null");
        return OAL_ERR_CODE_PTR_NULL;
    }
#endif
    /* 判断是否还需要继续进行扫描，如果此时扫描状态为非运行状态，说明扫描已经停止，无需再继续扫描 */
    if (MAC_SCAN_STATE_RUNNING != pst_mac_device->en_curr_scan_state)
    {
        OAM_WARNING_LOG0(pst_mac_device->st_scan_params.uc_vap_id, OAM_SF_SCAN,
                         "{dmac_scan_switch_home_channel_work_timeout::scan has been aborted, no need to continue.}");
        return OAL_SUCC;
    }

    dmac_scan_switch_channel_off(pst_mac_device);

    pst_mac_device->st_scan_params.en_working_in_home_chan = OAL_FALSE;

    /* 扫描继续进行时，根据是否为随机扫描，重新设置vap的mac addr */
    dmac_scan_set_vap_mac_addr_by_scan_state(pst_mac_device, OAL_TRUE);

    /* 清空信道测量结果 */
    OAL_MEMZERO(&(pst_mac_device->st_chan_result), OAL_SIZEOF(mac_scan_chan_stats_stru));
#ifdef _PRE_WLAN_FEATURE_BTCOEX
    if(HAL_BTCOEX_SW_POWSAVE_WORK == GET_HAL_BTCOEX_SW_PREEMPT_TYPE(pst_hal_device))
    {
        GET_HAL_BTCOEX_SW_PREEMPT_TYPE(pst_hal_device) = HAL_BTCOEX_SW_POWSAVE_SCAN_BEGIN;
        OAM_WARNING_LOG0(0, OAM_SF_COEX, "{dmac_btcoex_scan_begin:: normal scan begin delay by btcoex!}");
    }
    else
#endif
    {
        dmac_scan_begin(pst_mac_device);
    }
    return OAL_SUCC;
}


OAL_STATIC oal_void  dmac_scan_get_ch_statics_measurement_result(
                mac_device_stru                 *pst_mac_device)
{
    hal_ch_statics_irq_event_stru    st_stats_result;
    oal_uint8                        uc_chan_idx;

    /* 读取结果 */
    OAL_MEMZERO(&st_stats_result, OAL_SIZEOF(st_stats_result));
    hal_get_ch_statics_result(pst_mac_device->pst_device_stru, &st_stats_result);
    hal_get_ch_measurement_result(pst_mac_device->pst_device_stru, &st_stats_result);

#if defined(_PRE_WLAN_CHIP_TEST) && defined(_PRE_SUPPORT_ACS) && (_PRE_OS_VERSION_LINUX == _PRE_OS_VERSION)
    dmac_acs_channel_meas_comp_handler(pst_mac_device, &st_stats_result);
#endif

    uc_chan_idx  = pst_mac_device->uc_scan_chan_idx;

    /* 复制信道统计/测量结果信息 */
    pst_mac_device->st_chan_result.uc_stats_valid = 1;
    pst_mac_device->st_chan_result.uc_channel_number = pst_mac_device->st_scan_params.ast_channel_list[uc_chan_idx].uc_chan_number;

    if (st_stats_result.c_pri20_idle_power < 0)
    {
        pst_mac_device->st_chan_result.s_free_power_stats_20M  += (oal_int8)st_stats_result.c_pri20_idle_power; /* 主20M信道空闲功率 */
        pst_mac_device->st_chan_result.s_free_power_stats_40M  += (oal_int8)st_stats_result.c_pri40_idle_power; /* 主40M信道空闲功率 */
        pst_mac_device->st_chan_result.s_free_power_stats_80M  += (oal_int8)st_stats_result.c_pri80_idle_power; /* 全部80M信道空闲功率 */
        pst_mac_device->st_chan_result.uc_free_power_cnt += 1;
    }
    //else
    //{
    //    OAM_WARNING_LOG1(0, OAM_SF_SCAN, "{dmac_scan_get_ch_statics_measurement_result::c_pri20_idle_power=%u}", (oal_uint8)st_stats_result.c_pri20_idle_power);
    //}

    pst_mac_device->st_chan_result.ul_total_free_time_20M_us += st_stats_result.ul_pri20_free_time_us;
    pst_mac_device->st_chan_result.ul_total_free_time_40M_us += st_stats_result.ul_pri40_free_time_us;
    pst_mac_device->st_chan_result.ul_total_free_time_80M_us += st_stats_result.ul_pri80_free_time_us;
    pst_mac_device->st_chan_result.ul_total_recv_time_us     += st_stats_result.ul_ch_rx_time_us;
    pst_mac_device->st_chan_result.ul_total_send_time_us     += st_stats_result.ul_ch_tx_time_us;
    pst_mac_device->st_chan_result.ul_total_stats_time_us    += st_stats_result.ul_ch_stats_time_us;

}


oal_uint32 dmac_scan_channel_statistics_complete(frw_event_mem_stru *pst_event_mem)
{
    mac_device_stru                 *pst_mac_device;
    frw_event_stru                  *pst_event;
    oal_uint16                       us_total_scan_time_per_chan;
    oal_uint8                        uc_do_meas;        /* 本次扫描是否要获取信道测量的结果 */
    oal_uint8                        uc_chan_stats_cnt;

    //OAM_INFO_LOG0(0, OAM_SF_SCAN, "{dmac_scan_channel_statistics_complete:: channel statistic complete.}");

    /* 寻找对应的DEVICE结构以及相应的ACS结构 */
    pst_event = (frw_event_stru *)pst_event_mem->puc_data;
    pst_mac_device = mac_res_get_dev(pst_event->st_event_hdr.uc_device_id);
    if (OAL_PTR_NULL == pst_mac_device)
    {
        OAM_WARNING_LOG0(0, OAM_SF_SCAN, "{dmac_scan_channel_statistics_complete::pst_mac_device null.}");
        return OAL_ERR_CODE_PTR_NULL;
    }

    /* 读取硬件统计 测量结果 */
    uc_do_meas = pst_mac_device->st_scan_params.uc_scan_func & (MAC_SCAN_FUNC_MEAS | MAC_SCAN_FUNC_STATS);
    if (uc_do_meas)
    {
        dmac_scan_get_ch_statics_measurement_result(pst_mac_device);
    }

    /* 此信道硬件统计测量次数更新 */
    pst_mac_device->st_chan_result.uc_stats_cnt++;

    /* 每次硬件测量时间是10ms，根据本次扫描时长，确定要开启多少次硬件测量 */
    uc_chan_stats_cnt = pst_mac_device->st_chan_result.uc_stats_cnt;

    us_total_scan_time_per_chan = (oal_uint16)(pst_mac_device->st_scan_params.us_scan_time * pst_mac_device->st_scan_params.uc_max_scan_count_per_channel);
    if (uc_chan_stats_cnt * DMAC_SCAN_CHANNEL_MEAS_PERIOD_MS < us_total_scan_time_per_chan)
    {
        /* 再次启动一次测量 */
        hal_set_ch_statics_period(pst_mac_device->pst_device_stru, DMAC_SCAN_CHANNEL_STATICS_PERIOD_US);
        hal_set_ch_measurement_period(pst_mac_device->pst_device_stru, DMAC_SCAN_CHANNEL_MEAS_PERIOD_MS);
        hal_enable_ch_statics(pst_mac_device->pst_device_stru, 1);
    }

    return OAL_SUCC;
}


oal_void dmac_scan_radar_detected(mac_device_stru *pst_mac_device, hal_radar_det_event_stru *pst_radar_det_info)
{
    pst_mac_device->st_chan_result.uc_radar_detected = 1;
    pst_mac_device->st_chan_result.uc_radar_bw       = 0;
}



OAL_STATIC oal_uint32  dmac_scan_report_channel_statistics_result(mac_device_stru *pst_mac_device, oal_uint8 uc_scan_idx)
{
    frw_event_mem_stru         *pst_event_mem;
    frw_event_stru             *pst_event;
    dmac_crx_chan_result_stru  *pst_chan_result_param;

    /* 抛信道扫描结果到HMAC, 申请事件内存 */
    pst_event_mem = FRW_EVENT_ALLOC(OAL_SIZEOF(dmac_crx_chan_result_stru));
    if (OAL_PTR_NULL == pst_event_mem)
    {
        OAM_ERROR_LOG0(0, OAM_SF_SCAN, "{dmac_scan_report_channel_statistics_result::alloc mem fail.}");
        return OAL_ERR_CODE_PTR_NULL;
    }

    /* 填写事件 */
    pst_event = (frw_event_stru *)pst_event_mem->puc_data;

    FRW_EVENT_HDR_INIT(&(pst_event->st_event_hdr),
                       FRW_EVENT_TYPE_WLAN_CRX,
                       DMAC_WLAN_CRX_EVENT_SUB_TYPE_CHAN_RESULT,
                       OAL_SIZEOF(dmac_crx_chan_result_stru),
                       FRW_EVENT_PIPELINE_STAGE_1,
                       pst_mac_device->uc_chip_id,
                       pst_mac_device->uc_device_id,
                       0);

    pst_chan_result_param = (dmac_crx_chan_result_stru *)(pst_event->auc_event_data);

    pst_chan_result_param->uc_scan_idx    = uc_scan_idx;
    pst_chan_result_param->st_chan_result = pst_mac_device->st_chan_result;

    frw_event_dispatch_event(pst_event_mem);
    FRW_EVENT_FREE(pst_event_mem);

    return OAL_SUCC;
}



oal_uint32 dmac_scan_init(mac_device_stru *pst_device)
{
    /* 初始化device扫描状态为空闲 */
    pst_device->en_curr_scan_state = MAC_SCAN_STATE_IDLE;

    return OAL_SUCC;
}



oal_uint32 dmac_scan_exit(mac_device_stru *pst_device)
{
    return OAL_SUCC;
}

oal_uint32 dmac_sta_csa_stop_timeout_fn(void *arg)
{
    dmac_vap_stru *dmac_vap = (dmac_vap_stru *)(arg);
    if (dmac_vap->st_vap_base_info.en_vap_state == MAC_VAP_STATE_BUTT) {
        return OAL_SUCC;
    }
    OAM_WARNING_LOG0(dmac_vap->st_vap_base_info.uc_vap_id, 0, "{dmac_sta_csa_stop_timeout_fn::timer timeout!}");
    dmac_vap->st_vap_base_info.st_ch_switch_info.uc_switch_fail = OAL_FALSE;
    FRW_TIMER_DESTROY_TIMER(&g_csa_stop_timer);

    return OAL_SUCC;
}

/*lint -e19 */

oal_module_symbol(dmac_scan_switch_channel_off);
oal_module_symbol(dmac_scan_begin);
oal_module_symbol(dmac_scan_abort);
oal_module_symbol(dmac_scan_switch_channel_back);
#ifdef _PRE_WLAN_FEATURE_CCA_OPT
oal_module_symbol(dmac_scan_handle_scan_req_entry);
#endif

/*lint +e19 */
#ifdef __cplusplus
    #if __cplusplus
        }
    #endif
#endif
