// extractorDlg.h : header file
//

#pragma once
#include "afxcmn.h"
#include "resource.h"
#include "entity.h"


// CextractorDlg dialog
class CextractorDlg : public CDialog
{
// Construction
public:
	CextractorDlg(CWnd* pParent = NULL);	// standard constructor

    void print_status(CString status);
// Dialog Data
	enum { IDD = IDD_EXTRACTOR_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

    void init_task_list();
    void add_task(Task* task);

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
    afx_msg void OnDestroy();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
    afx_msg void OnBnClickedReset();
    afx_msg void OnBnClickedExtract();
    afx_msg void OnBnClickedBrowseFolder();
    CListCtrl m_task_list;
    int m_running_task_num;
};

unsigned __stdcall monitor_wrapper(void* pArguments);
