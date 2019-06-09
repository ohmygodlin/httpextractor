// extractorDlg.cpp : implementation file
//

#include "stdafx.h"
#include "extractor.h"
#include "extractorDlg.h"

#include "shlwapi.h"
#pragma comment(lib, "shlwapi.lib")

#include "comdef.h"
#pragma comment(lib, "comsuppw.lib")

#include<process.h>
#include<winsock2.h>
#pragma comment(lib, "Ws2_32.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()


// CextractorDlg dialog




CextractorDlg::CextractorDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CextractorDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CextractorDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_TASKS, m_task_list);
}

BEGIN_MESSAGE_MAP(CextractorDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
    ON_WM_DESTROY()
	//}}AFX_MSG_MAP
    ON_BN_CLICKED(IDC_RESET, &CextractorDlg::OnBnClickedReset)
    ON_BN_CLICKED(IDC_EXTRACT, &CextractorDlg::OnBnClickedExtract)
    ON_BN_CLICKED(IDC_BROWSE_FOLDER, &CextractorDlg::OnBnClickedBrowseFolder)
END_MESSAGE_MAP()


// CextractorDlg message handlers

BOOL CextractorDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here
    WSADATA wsaData;
    int err = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (err != NO_ERROR)
    {
        Util::print_error(err);
        exit(err);
    }
    init_task_list();
    this->m_running_task_num = 0;
    _beginthreadex(NULL, 0, monitor_wrapper, NULL, 0, NULL);

    //For test.
    SetDlgItemText(IDC_URL, TEXT("http://s0.cyberciti.org/images/misc/static/2012/11/ifdata-welcome-0.png"));

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CextractorDlg::init_task_list()
{
    this->m_task_list.SetExtendedStyle(LVS_EX_GRIDLINES|LVS_EX_FULLROWSELECT|LVS_EX_CHECKBOXES|LVS_EX_AUTOCHECKSELECT|LVS_EX_AUTOSIZECOLUMNS);
    this->m_task_list.InsertColumn(0, TEXT("File"), LVCFMT_LEFT, 90);
    this->m_task_list.InsertColumn(this->m_task_list.GetHeaderCtrl()->GetItemCount(), TEXT("Progress"), LVCFMT_LEFT, 90);
    this->m_task_list.InsertColumn(this->m_task_list.GetHeaderCtrl()->GetItemCount(), TEXT("Status"), LVCFMT_LEFT, 90);
}

void CextractorDlg::OnDestroy()
{
    CDialog::OnDestroy();
    WSACleanup();
}

void CextractorDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CextractorDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CextractorDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CextractorDlg::OnBnClickedReset()
{
    // Clear Edit Control.
    SetDlgItemText(IDC_URL, _T(""));
    SetDlgItemText(IDC_LOCAL_FOLDER, _T(""));
    SetDlgItemText(IDC_STATUS, _T(""));
}

void CextractorDlg::OnBnClickedExtract()
{
    // Extract url file to local folder.
    TCHAR url_str[MAX_PATH], folder[MAX_PATH];
    GetDlgItemText(IDC_URL, url_str, MAX_PATH);
    GetDlgItemText(IDC_LOCAL_FOLDER, folder, MAX_PATH);
    CString strMsg;

    Url* url = Url::gen_url(url_str);
    if (url == NULL)
    {
        strMsg.Format(TEXT("URL: \"%s\" is invalid url!"), url_str);
        print_status(strMsg);
        return ;
    }

    // Judge whether local folder exists.
    if (! PathIsDirectory(folder))
    {
        strMsg.Format(TEXT("Folder \"%s\" does not exist, create?"), folder);
        if (MessageBox(strMsg,_T("Confirm"), MB_YESNO) == IDYES)
        {
            if (! ::CreateDirectory(folder, NULL))
            {
                strMsg.Format(TEXT("Fail to create folder \"%s\", please specify another folder."), folder);
                print_status(strMsg);
                return ;
            }
        }
        else
            return;
    }

    Task* task = new Task(url, folder);
    this->add_task(task);
/*    
    _tcscat(folder, _T("\\"));
    _tcscat(folder, ++file_name);
    HRESULT rt = URLDownloadToFile(NULL, url, path, 0, NULL);
    if (rt == S_OK)
    {
        MessageBox(_T("Download successfully!"));
    }
    else
    {
        _com_error error(rt);
        LPCTSTR errorText = error.ErrorMessage();
        MessageBox(errorText, _T("Download error!"));
    }
*/
}

void CextractorDlg::add_task(Task* task)
{
    if (this->m_running_task_num >= Util::g_max_running_task_num)
    {
        task->set_status(ETR_STATUS_PENDING);
    }
    else
    {
        this->m_running_task_num ++;
        task->set_status(ETR_STATUS_RUNNING);
        task->process();
    }
    int item_count = this->m_task_list.GetItemCount();
    CString msg;
    msg.Format(TEXT("Count: %i, Status: %s"), item_count, ETR_STATUS_STR[task->m_status]);
    print_status(msg);
    this->m_task_list.InsertItem(item_count, task->m_base_name);
    this->m_task_list.SetItemData(item_count, (DWORD_PTR)task);
    this->m_task_list.SetItemText(item_count, 2, ETR_STATUS_STR[task->m_status]);
}

void CextractorDlg::OnBnClickedBrowseFolder()
{
    BROWSEINFO bi;
    ZeroMemory(&bi, sizeof(BROWSEINFO));
//    bi.hwndOwner = AfxGetApp()->GetMainWnd()->m_hWnd;
    bi.hwndOwner = this->m_hWnd;
    bi.ulFlags = BIF_RETURNONLYFSDIRS;
    LPITEMIDLIST pidl = ::SHBrowseForFolder(&bi);
    BOOL b = FALSE;
    TCHAR folder[MAX_PATH];
    folder[0] = _T('\0');
    if (pidl)
    {
        if (::SHGetPathFromIDList(pidl, folder))
            b = TRUE;
        IMalloc *p = NULL;
        if (SUCCEEDED(::SHGetMalloc(&p)) && p)
        {
            p->Free(pidl);
            p->Release();
        }
    }
    if (b)
        SetDlgItemText(IDC_LOCAL_FOLDER, folder);
//        GetDlgItem(IDC_LOCAL_FOLDER)->SetWindowText(folder);
}

void CextractorDlg::print_status(CString status)
{
    status += TEXT("\n");
    CEdit* ce = (CEdit*)GetDlgItem(IDC_STATUS);
    ce->SetSel(-1);
    ce->ReplaceSel(status);
}

unsigned __stdcall monitor_wrapper(void* pArguments)
{
    Monitor::singleton()->monitor();
    return 0;
}
      
/* Format last error.
        LPVOID lpMsgBuf;
        ::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM, 0, GetLastError(), 
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);
        MessageBox((LPTSTR)lpMsgBuf, _T("Download error!"));
        LocalFree(lpMsgBuf);
*/