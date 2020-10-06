// maindlg.cpp : implementation of the CMainDlg class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "maindlg.h"

#include "config.h"


LRESULT CMainDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	// center the dialog on the screen
	CenterWindow();


	// set icons
	HICON hIcon = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
		IMAGE_ICON, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
	SetIcon(hIcon, TRUE);
	HICON hIconSmall = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
		IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
	SetIcon(hIconSmall, FALSE);

    // First DDX call, hooks up variables to controls.
    DoDataExchange(false);

    m_opc = new ic_opcdaclient();

    SetTimer(1,1000);

    

	return TRUE;
}

LRESULT CMainDlg::OnTimer(UINT_PTR id){
    if(id == 1){
        CString cs;
        const std::vector<opc_item>* items = m_opc->items();
        if(items->size()>0) {
            
            char buf[512];
            memset(buf, 0, sizeof(buf));
            const opc_item& item = items->at(0);
            CComVariant value = items->at(0).value;
            value.ChangeType(VT_R8);
            sprintf(buf, "%d. name: %s value: %lf", id, item.name, value.dblVal);

            
            //memset(cs.GetBufferSetLength(512), 0, sizeof(wchar_t)*512);

            
            MultiByteToWideChar(CP_ACP, 0, buf, strlen(buf)+1, cs.GetBufferSetLength(512), 512);
            cs.ReleaseBuffer();
            //wsprintf(buff+wcslen(buff), L" value %f", item.value.dblVal);
            //cs.Format(L"%d name:%s value %lf", id, buff, items->at(0).value.dblVal);
            //cs = buff;
            m_ShowItem.SetWindowText(cs);
            
        }
    }
    return 0;
}

LRESULT CMainDlg::OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CSimpleDialog<IDD_ABOUTBOX, FALSE> dlg;
	dlg.DoModal();
	return 0;
}

LRESULT CMainDlg::OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	// TODO: Add validation code 
    delete m_opc;

	EndDialog(wID);
	return 0;
}

LRESULT CMainDlg::OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    config_file cfg;
    cfg.load_file("Z:\\work\\bj_wwhy\\code\\icopc\\docs\\config.json");

    char* json = NULL;
    cfg.dump(&json);
    MessageBoxA(NULL, json, 0, MB_OK);
    free(json);

    char merge_str[]=
        "{"
	"\"version\":\"1.0\","
	"\"connections\":["
		"{"
		"\"active\":1,"
		"\"host\":\"127.0.0.1\","
		"\"server\":\"Matrikon.OPC.Simulation\","
		"\"groups\":["
			"{"
			"\"active\":1,"
			"\"group\":\"group_append\","
			"\"rate\":5000,"
			"\"dead_band\":0.0,"
            "\"items\":["
                "{"
                "\"active\":0,"
				"\"item\":\"Random.Var4\","
				"\"type\":\"VT_R8\""
                "}"
                "]"
            "}"
            "]"
        "}"
    "]"
"}"
            ;
    config_file cfg_merge;
    cfg_merge.load(merge_str);
    cfg.merge(&cfg_merge);

    cfg.dump(&json);
    MessageBoxA(NULL, json, 0, MB_OK);
    free(json);

    char diff_str[]=
        "{"
	"\"version\":\"1.0\","
	"\"connections\":["
		"{"
		"\"active\":1,"
		"\"host\":\"127.0.0.1\","
		"\"server\":\"Matrikon.OPC.Simulation\","
		"\"groups\":["
			"{"
			"\"active\":1,"
			"\"group\":\"group_0\","
			"\"rate\":1000,"
			"\"dead_band\":0.0,"
			"\"items\":["
				"{"
				"\"active\":1,"
				"\"item\":\"Random.Int4\","
				"\"type\":\"VT_R8\""
				"}"
			"]"	
			"}"
		"]"
		"}"
	"]"
"}"
;

    config_file cfg_diff;
    cfg_diff.load(diff_str);

    cfg.diff(&cfg_diff);

    cfg.dump(&json);
    MessageBoxA(NULL, json, 0, MB_OK);
    free(json);


    printf("call connect\n");

    int svrs = NumberOfOPCServers(FALSE, "127.0.0.1");
    char buf[512]={0};
    int items;
    if(svrs == 0) {
        memset(buf, 0, sizeof(buf));
        sprintf(buf, "server number %d", svrs);
        MessageBoxA(NULL, buf, NULL, MB_OK);
    }

    for(int i=0; i<svrs; ++i) {
        memset(buf, 0, sizeof(buf));
        sprintf(buf, "server %d name:", i);
        size_t sz =strlen(buf);
        GetOPCServerName(i, buf+sz, sizeof(buf)-sz);
        MessageBoxA(NULL, buf, NULL, MB_OK);
    }

    if(svrs == 0) return 0;

    // 连接OPC Server
    HANDLE opc = ConnectOPC("127.0.0.1", "Matrikon.OPC.Simulation", FALSE);

    if(opc==NULL) return 0;

    items = NumberOfOPCItems(opc);
    if(items==0) {
    memset(buf,0, sizeof(buf));
    sprintf(buf, "items: %d", items);
    

    MessageBoxA(NULL,buf,NULL,MB_OK);
    }
    if(opc)
        DisconnectOPC(opc);

    int server = m_opc->connect("localhost","Matrikon.OPC.Simulation");
    int group = m_opc->add_group("grp", 1000, 0, server);
    m_opc->add_item("Random.Int4", group, server);
    /*
    int svrs = NumberOfOPCServers(FALSE, "127.0.0.1");
    char buf[512]={0};
    int items;
    if(svrs == 0) {
        memset(buf, 0, sizeof(buf));
        sprintf(buf, "server number %d", svrs);
        MessageBoxA(NULL, buf, NULL, MB_OK);
    }

    for(int i=0; i<svrs; ++i) {
        memset(buf, 0, sizeof(buf));
        sprintf(buf, "server %d name:", i);
        size_t sz =strlen(buf);
        GetOPCServerName(i, buf+sz, sizeof(buf)-sz);
        MessageBoxA(NULL, buf, NULL, MB_OK);
    }

    if(svrs == 0) goto CLEAN_END;

    // 连接OPC Server
    m_opc = ConnectOPC("127.0.0.1", "Matrikon.OPC.Simulation.1", TRUE);

    if(m_opc==NULL) goto CLEAN_END;

    items = NumberOfOPCItems(m_opc);
    if(items==0) {
    memset(buf,0, sizeof(buf));
    sprintf(buf, "items: %d", items);
    

    MessageBoxA(NULL,buf,NULL,MB_OK);
    }

    for(i=0;i<items,i<1;++i) {
        memset(buf,0, sizeof(buf));
        sprintf(buf, "total[%d] item %d name:", items, i);
        size_t sz = strlen(buf);
        GetOPCItemName(m_opc, i, buf+sz, sizeof(buf)-sz);
        MessageBoxA(NULL,buf,NULL,MB_OK);

    }
    if(items ==0)goto CLEAN_END;
    if(items>0) {
    DWORD rate = 1000; //ms 
    float dead_band=0;
    char group[]="Group0";
    char name[]="Random.Int4";
    m_group = AddOPCGroup(m_opc, group, &rate, &dead_band);
    {
        memset(buf,0, sizeof(buf));
        size_t sz = strlen(buf);
        sprintf(buf, "group %s rate:%d band:%f", group, rate, dead_band);
        MessageBoxA(NULL, buf, NULL, MB_OK);
    }
    ChangeOPCGroupState(m_opc, m_group, TRUE);

     m_item = AddOPCItem(m_opc, m_group, name);
    {
        //memset(buf,0, sizeof(buf));
        //size_t sz = strlen(buf);
        //sprintf(buf, "item %s add %p", name, m_item);
        //MessageBoxA(NULL, buf, NULL, MB_OK);
    }
    MessageBoxA(NULL,NULL,NULL,MB_OK);
    //Sleep(5000);

    CComVariant var;
    FILETIME tm;
    DWORD quality;
    ReadOPCItem(m_opc, m_group, m_item, &var, &tm, &quality);

    memset(buf, 0, sizeof(buf));
    sprintf(buf, "var[%d] %s = ", var.vt, name);
    size_t sz = strlen(buf);

    switch(var.vt){
        case VT_EMPTY:
		case VT_NULL:
            break;
        case VT_BOOL:
            sprintf(buf+sz, "VT_BOOL %d", var.boolVal);
            break;
        case VT_UI1: 
            sprintf(buf+sz, "VT_UI1 %d", var.bVal);
            break;
        case VT_I2: 
            sprintf(buf+sz, "VT_I2 %d", var.iVal);
            break;
        case VT_I4:
            sprintf(buf+sz, "VT_I4 %d", var.lVal);
            break;
        case VT_R4:
            sprintf(buf+sz, "VT_R4 %f", var.fltVal);
            break;
        case VT_R8:
            sprintf(buf+sz, "VT_R8 %lf", var.dblVal);
            break;
        case VT_BSTR:
            sprintf(buf+sz, "VT_BSTR"); //, var.bstrVal
            break;
        case VT_ERROR:
            sprintf(buf+sz, "VT_ERROR %d", var.scode);
            break;
        case VT_DISPATCH:
            sprintf(buf+sz, "VT_DISPATCH"); //var.pdispVal;
            break;
        case VT_UNKNOWN:
            sprintf(buf+sz, "VT_UNKNOWN"); //var.punkVal;
            break;
        default:
            break;
    }
    //MessageBoxA(NULL, buf, NULL, MB_OK);
    }
    
    //EnableOPCNotificationEx(m_opc, opc_update_proc);
    

CLEAN_END:
    
	//EndDialog(wID);
    */
	return 0;
}
