// CHtmlCtrl 实现 -- 控件中的 web 浏览器，只要改写 CHtmlVie
// 你就可以摆脱框架 - 从而将此控制用于对话框和其它任何窗口。
//
// 特性s:
// - SetCmdMap 用于设置“app:command”链接的命令映射。.
// - SetHTML 用于将字符串转换为 HTML 文档。.

#include "StdAfx.h"
#include "HtmlCtrl.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define WM_ON_CLICK_LINK WM_USER + 10000
#define WM_ON_EXTERNAL_FOO WM_USER + 10001

// useful macro for checking HRESULTs
#define HRCHECK(x) \
	hr = x; if (!SUCCEEDED(hr)) \
	{ \
		TRACE(_T("hr=%p\n"),hr);\
		return hr;\
	}

#define DISP_FUNCTION(theClass, szExternalName, pfnMember, vtRetVal, vtsParams) \
{ _T(szExternalName), DISPID_UNKNOWN, vtsParams, vtRetVal, \
	(AFX_PMSG)(void (theClass::*)(void))&pfnMember, (AFX_PMSG)0, 0, \
	afxDispCustom }, 

CHtmlCtrl::CHtmlCtrl()
	:m_bHideMenu(FALSE)
{
	EnableAutomation();
	AppendFunction(_T("call"), (AFX_PMSG)&CHtmlCtrl::OnScriptExternalCall, VT_EMPTY, VTS_BSTR VTS_BSTR);
}

const AFX_DISPMAP* PASCAL CHtmlCtrl::GetThisDispatchMap()
{
	return &CHtmlCtrl::dispatchMap;
}
const AFX_DISPMAP* CHtmlCtrl::GetDispatchMap() const
{
	return &CHtmlCtrl::dispatchMap;
}

AFX_COMDAT AFX_DISPMAP_ENTRY CHtmlCtrl::_dispatchEntries[MAX_FUNCS] =
{
	{ VTS_NONE, DISPID_UNKNOWN, VTS_NONE, VT_VOID, (AFX_PMSG)NULL, (AFX_PMSG)NULL, (size_t)-1, afxDispCustom }
};
AFX_COMDAT const AFX_DISPMAP CHtmlCtrl::dispatchMap =
{
	(const AFX_DISPMAP *)(&CHtmlCtrl::GetThisDispatchMap),
	&CHtmlCtrl::_dispatchEntries[0],
	&CHtmlCtrl::_dispatchEntryCount, 
	&CHtmlCtrl::_dwStockPropMask 
};
AFX_COMDAT UINT CHtmlCtrl::_dispatchEntryCount = (UINT)0; //绑定的c++函数数量
AFX_COMDAT DWORD CHtmlCtrl::_dwStockPropMask = (DWORD)-1;

IMPLEMENT_DYNAMIC(CHtmlCtrl, CHtmlView)

LRESULT CHtmlCtrl::OnClickLink(WPARAM pProtocol, LPARAM pCmd)
{
	CString* pszProtocol = (CString*)pProtocol;
	pszProtocol->MakeLower();
	CString* pszCmd = (CString*)pCmd;

	if (m_mpOnClickLink[*pszProtocol][*pszCmd])
		m_mpOnClickLink[*pszProtocol][*pszCmd](*pszProtocol, *pszCmd);

	delete pszProtocol;
	delete pszCmd;
	return 0;
}

LRESULT CHtmlCtrl::OnExternalCall(WPARAM pFuncName, LPARAM pJsonStr)
{
	CString* pszFuncName = (CString*)pFuncName;
	CString* pszJsonStr = (CString*)pJsonStr;

	std::map<CString, FN_ON_EXTERNAL_CALL>::iterator itFinder = m_mpExternalCall.find(*pszFuncName);
	if (itFinder != m_mpExternalCall.end() && itFinder->second)
		itFinder->second(*pszJsonStr);

	delete pszFuncName;
	delete pszJsonStr;
	return 0;
}

void CHtmlCtrl::OnScriptExternalCall(LPCTSTR pFunName, LPCTSTR pJsonStr)
{
	PostMessage(WM_ON_EXTERNAL_FOO, WPARAM(new CString(pFunName)), LPARAM(new CString(pJsonStr)));
}

BEGIN_MESSAGE_MAP(CHtmlCtrl, CHtmlView)
	ON_WM_DESTROY()
	ON_WM_MOUSEACTIVATE()
	ON_MESSAGE(WM_ON_CLICK_LINK, &CHtmlCtrl::OnClickLink)
	ON_MESSAGE(WM_ON_EXTERNAL_FOO, &CHtmlCtrl::OnExternalCall)
END_MESSAGE_MAP()

//////////////////
// Create control in same position as an existing static control with given
// the same ID (could be any kind of control, really)
//
BOOL CHtmlCtrl::CreateFromStatic(UINT nID, CWnd* pParent, DWORD dwStyle/*=WS_CHILD|WS_VISIBLE*/, CCreateContext* pContext/*=NULL*/)
{
	CStatic wndStatic;
	if (!wndStatic.SubclassDlgItem(nID, pParent))
		return FALSE;
	// Get static control rect, convert to parent's client coords.
	CRect rc;
	wndStatic.GetWindowRect(&rc);
	pParent->ScreenToClient(&rc);
	//wndStatic.DestroyWindow();

	BOOL bRet = Create(rc, pParent, nID);
	return bRet;
}

////////////////
// Override to avoid CView stuff that assumes a frame.
//
void CHtmlCtrl::OnDestroy()
{
	m_pBrowserApp = NULL; // will call Release
	CWnd::OnDestroy(); // bypass CView doc/frame stuff
}

////////////////
// Override to avoid CView stuff that assumes a frame.
//
int CHtmlCtrl::OnMouseActivate(CWnd* pDesktopWnd, UINT nHitTest, UINT msg)
{
	// bypass CView doc/frame stuff
	return CWnd::OnMouseActivate(pDesktopWnd, nHitTest, msg);
}

// Return TRUE iff hwnd is internet explorer window.
inline BOOL IsIEWindow(HWND hwnd)
{
	static LPCTSTR IEWNDCLASSNAME = _T("Internet Explorer_Server");
	TCHAR classname[32]; // always char, never TCHAR

	GetClassName(hwnd, (LPTSTR)classname, sizeof(classname) / sizeof(TCHAR));
	return _tcscmp(classname, IEWNDCLASSNAME) == 0;
}

//////////////////
// Override to trap "Internet Explorer_Server" window context menu messages.
//
BOOL CHtmlCtrl::PreTranslateMessage(MSG* pMsg)
{
	if (m_bHideMenu)
	{//屏蔽右键
		switch (pMsg->message)
		{
		case WM_CONTEXTMENU:
		case WM_RBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONDBLCLK:
			if (IsIEWindow(pMsg->hwnd))
			{
				if (pMsg->message == WM_RBUTTONUP)
					// let parent handle context menu
					GetParent()->SendMessage(WM_CONTEXTMENU, pMsg->wParam, pMsg->lParam);
				return TRUE; // eat it
			}
		}
	}

	return CHtmlView::PreTranslateMessage(pMsg);
}

//////////////////
// Override to pass "app:" links to virtual fn instead of browser.
//
void CHtmlCtrl::OnBeforeNavigate2(LPCTSTR lpszURL,
	DWORD nFlags, LPCTSTR lpszTargetFrameName, CByteArray& baPostedData,
	LPCTSTR lpszHeaders, BOOL* pbCancel)
{
	//不处理空白页
	if (_tcsicmp(_T("about:blank"), lpszURL) == 0)
		return;
	//不处理来自SetHtml的页面
	if (_tcsicmp(_T("about:text/html"), lpszURL) == 0)
		return;

	CString sProtocol, sCmd;
		
	LPCTSTR ABOUT_PROTOCOL = _T("about:");
	if (_tcsnicmp(lpszURL, ABOUT_PROTOCOL, (int)_tcslen(ABOUT_PROTOCOL)) == 0)
	{//处理点击没有协议的链接
		sProtocol = _T("");
		sCmd = lpszURL + (int)_tcslen(ABOUT_PROTOCOL);
	}
	else
	{//处理点击有协议的链接
		SplitProtocol(lpszURL, sProtocol, sCmd);
	}

	std::map<CString, std::map<CString, FN_ON_CLICK_LINK> >::iterator itProtocol =
		m_mpOnClickLink.find(sProtocol);
	if (itProtocol == m_mpOnClickLink.end())
		return ;
	std::map<CString, FN_ON_CLICK_LINK>::iterator itCmd = itProtocol->second.find(sCmd);
	if (itCmd == itProtocol->second.end())
		return ;

	PostMessage(WM_ON_CLICK_LINK, WPARAM(new CString(sProtocol)), LPARAM(new CString(sCmd)));
	*pbCancel = TRUE;
}

//////////////////
// Set document contents from string
//
BOOL CHtmlCtrl::SetHtml(LPCTSTR strHTML)
{
	// Get document object
	SPIHTMLDocument2 doc = GetHtmlDocument();

	if (doc.p == NULL)
	{//内容为空时创建空白页
		Navigate(_T("about:blank"));
		doc = GetHtmlDocument();
		if (doc.p == NULL)
			return FALSE;
	}

	BOOL bRet = FALSE;

	// Create string as one-element BSTR safe array for IHTMLDocument2::write.
	CComSafeArray<VARIANT> sar;
	sar.Create(1, 0);
	sar[0] = CComBSTR(strHTML);
	// open doc and write
	LPDISPATCH lpdRet;
	if (SUCCEEDED(doc->open(CComBSTR("text/html"),
		CComVariant(CComBSTR("_self")),
		CComVariant(CComBSTR("")),
		CComVariant((bool)1),
		&lpdRet)))
	{
		if (SUCCEEDED(doc->write(sar))) // write contents to doc
		{
			bRet = TRUE;
		}
		doc->close(); // close

		lpdRet->Release(); // release IDispatch returned
	}
	
	return bRet;
}

void CHtmlCtrl::OnNavigateComplete2(LPCTSTR strURL)
{
	//屏闭脚本错误框的弹出
	CComPtr<IDispatch> spDisp = GetSafeHtmlDocument();
	if (NULL != spDisp)
	{
		CComPtr<IHTMLDocument2> doc;
		spDisp->QueryInterface(IID_IHTMLDocument2, reinterpret_cast<void**>(&doc));
		if (NULL != doc)
		{
			CScriptErrorHandler scriptHandler;
			scriptHandler.Run(doc);
		}
	}

	CHtmlView::OnNavigateComplete2(strURL);
}

void CHtmlCtrl::OnNavigateError(LPCTSTR lpszURL, LPCTSTR lpszFrame, DWORD dwError, BOOL *pbCancel)
{
	*pbCancel = TRUE;
}

BOOL CHtmlCtrl::Create(const RECT& rc, CWnd* pParent, UINT nID, DWORD dwStyle /*= WS_CHILD|WS_VISIBLE*/, CCreateContext* pContext /*= NULL*/)
{
	return CHtmlView::Create(NULL, NULL, dwStyle, rc, pParent, nID, pContext);
}

HRESULT CHtmlCtrl::OnGetExternal(LPDISPATCH *lppDispatch)
{
	*lppDispatch = GetIDispatch(TRUE);
	return S_OK;
}

BOOL CHtmlCtrl::AppendFunction(LPCTSTR pszFuncName, AFX_PMSG pFuncAddr, WORD wRetType, LPCSTR pszParamType)
{
	if (_dispatchEntryCount >= MAX_FUNCS)
		return FALSE;

	_dispatchEntries[_dispatchEntryCount].lpszName = pszFuncName;
	_dispatchEntries[_dispatchEntryCount].lDispID = DISPID_UNKNOWN;
	_dispatchEntries[_dispatchEntryCount].lpszParams = pszParamType;
	_dispatchEntries[_dispatchEntryCount].vt = wRetType;
	_dispatchEntries[_dispatchEntryCount].pfn = pFuncAddr;
	_dispatchEntries[_dispatchEntryCount].pfnSet = (AFX_PMSG)0;
	_dispatchEntries[_dispatchEntryCount].nPropOffset = 0;
	_dispatchEntries[_dispatchEntryCount].flags = afxDispCustom;

	++_dispatchEntryCount;
	return TRUE;
}

void CHtmlCtrl::AppendFunction(LPCTSTR pszFuncName, FN_ON_EXTERNAL_CALL pfn)
{
	m_mpExternalCall[pszFuncName] = pfn;
}

BOOL CHtmlCtrl::AppendOnClickLink(const CString& sProtocol, const CString& sCmd, FN_ON_CLICK_LINK fn)
{
	if (!IsProtocolLegal(sProtocol))
		return FALSE;

	CString s = sProtocol;
	s.MakeLower();
	m_mpOnClickLink[s][sCmd] = fn;

	return TRUE;
}

bool CHtmlCtrl::SplitProtocol(const CString& src, CString& sProtocol, CString& sCmd)
{
	int nIdxMao = src.Find(':', 0);
	if (nIdxMao < 0)
		return false;
	sProtocol = src.Mid(0, nIdxMao);
	sCmd = src.Mid(nIdxMao + 1, src.GetLength() - nIdxMao - 1);
	return true;
}

bool CHtmlCtrl::IsProtocolLegal(const CString& sProtocol)
{
	//检测是否合法
	//  0~9 a~z + - . 为合法
	for (int i = 0; i < sProtocol.GetLength(); ++i)
	{
		if (!(sProtocol[i] >= '0' && sProtocol[i] <= '9') &&
			!(sProtocol[i] >= 'a' && sProtocol[i] <= 'z') &&
			sProtocol[i] != '+' && sProtocol[i] != '-' && sProtocol[i] != '.')
		{
			return false;
		}
	}

	return true;
}

SPIHTMLDocument2 CHtmlCtrl::GetSafeHtmlDocument() const
{
	IDispatch* pDIs = NULL;
	if (SUCCEEDED(m_pBrowserApp->get_Document(&pDIs)))
	{
		SPIHTMLDocument2 pDocument = (SPIHTMLDocument2)pDIs;
		return pDocument;
	}
	return (SPIHTMLDocument2)pDIs;
}

HRESULT CHtmlCtrl::OnGetHostInfo(DOCHOSTUIINFO *pInfo)
{
	//如果窗口够大的话则不用滚动条,如果页面比窗口大,滚动条还是会出来的
	//pInfo->dwFlags |= DOCHOSTUIFLAG_SCROLL_NO | DOCHOSTUIFLAG_NO3DBORDER;
	return S_OK;
}

void CHtmlCtrl::OnNewWindow2(LPDISPATCH* ppDisp, BOOL* Cancel)
{
	//实现当需要新建窗口时也在同一个窗口加载
	CComPtr<IHTMLDocument2> pHTMLDocument2;
	m_pBrowserApp->get_Document((IDispatch **)&pHTMLDocument2);
	if (pHTMLDocument2 != NULL)
	{
		CComPtr<IHTMLElement> pIHTMLElement;
		pHTMLDocument2->get_activeElement(&pIHTMLElement);

		if (pIHTMLElement != NULL)
		{
			variant_t url;
			HRESULT hr = pIHTMLElement->getAttribute(L"href", 0, &url);
			if (SUCCEEDED(hr))
			{
				hr = m_pBrowserApp->Navigate2(&url, NULL, NULL, NULL, NULL);
				url.Clear();
			}
		}
	}
	*Cancel = TRUE;
}

BOOL CHtmlCtrl::ExecuteScript(const CString &sScript, const CString &sLanguage/*=_T("JavaScript")*/)
{
	BOOL bRet = FALSE;
	CComPtr<IDispatch> spDisp = GetSafeHtmlDocument();
	if (NULL != spDisp)
	{
		CComPtr<IHTMLDocument2> doc;
		spDisp->QueryInterface(IID_IHTMLDocument2, reinterpret_cast<void**>(&doc));
		if (NULL != doc)
		{			
			bRet = ExecuteScript(doc, sScript, sLanguage);
		}
	}

	return bRet;
}

BOOL CHtmlCtrl::ExecuteScript(CComVariant& res, const CString &sScript, 
	const CString &sLanguage/*=_T("JavaScript")*/)
{
	BOOL bRet = FALSE;
	CComPtr<IDispatch> spDisp = GetSafeHtmlDocument();
	if (NULL != spDisp)
	{
		CComPtr<IHTMLDocument2> doc;
		spDisp->QueryInterface(IID_IHTMLDocument2, reinterpret_cast<void**>(&doc));
		if (NULL != doc)
		{
			bRet = ExecuteScript(doc, res, sScript, sLanguage);
		}
	}

	return bRet;
}

BOOL CHtmlCtrl::ExecuteScript(CComPtr<IHTMLDocument2> &doc, const CString &sScript, 
	const CString &sLanguage /*= _T("JavaScript")*/)
{
	BOOL bRet = FALSE;
	CComPtr<IHTMLWindow2>  spIhtmlwindow2;
	doc->get_parentWindow(reinterpret_cast<IHTMLWindow2**>(&spIhtmlwindow2));
	if (spIhtmlwindow2 != NULL)
	{
		BSTR bstrScript = sScript.AllocSysString();
		BSTR bstrLanguage = sLanguage.AllocSysString();
		VARIANT ret;
		if (SUCCEEDED(spIhtmlwindow2->execScript(bstrScript, bstrLanguage, &ret)))
			bRet = TRUE;
		::SysFreeString(bstrScript);
		::SysFreeString(bstrLanguage);
	}

	return bRet;
}

BOOL CHtmlCtrl::ExecuteScript(CComPtr<IHTMLDocument2> &doc, CComVariant& res, 
	const CString &sScript, const CString &sLanguage /*= _T("JavaScript")*/)
{
	CString sScriptStr = sScript;
	sScriptStr.Replace(_T("\""), _T("\\\""));
	sScriptStr.Replace(_T("'"), _T("\\'"));
	CString sBridge;
	sBridge.Format(_T("function __temp__(){return eval(\"%s\");}"), sScriptStr);
	if (ExecuteScript(doc, sBridge, sLanguage) != TRUE)
		return FALSE;
	return CallScriptFunction(doc, _T("__temp__"), NULL, &res);
}

BOOL CHtmlCtrl::CallScriptFunction(CComPtr<IHTMLDocument2> &doc, LPCTSTR pStrFuncName, 
	CStringArray* pArrFuncArgs /*= NULL*/, CComVariant* pOutVarRes /*= NULL*/)
{
	//Call client function in HTML
	//'pStrFuncName' = client script function name
	//'pArrFuncArgs' = if not NULL, list of arguments
	//'pOutVarRes' = if not NULL, will receive the return value
	//RETURN: = TRUE if done

	BOOL bRes = FALSE;
	CComVariant vaResult;
	
	//Getting IDispatch for Java Script objects
	CComPtr<IDispatch> spScript;
	if (SUCCEEDED(doc->get_Script(&spScript)))
	{
		//Find dispid for given function in the object
		CComBSTR bstrMember(pStrFuncName);
		DISPID dispid = NULL;
		if (SUCCEEDED(spScript->GetIDsOfNames(IID_NULL, &bstrMember, 1, 
			LOCALE_USER_DEFAULT, &dispid)))
		{
			const int arraySize = pArrFuncArgs ? pArrFuncArgs->GetSize() : 0;

			//Putting parameters  
			DISPPARAMS dispparams;
			memset(&dispparams, 0, sizeof dispparams);
			dispparams.cArgs = arraySize;
			dispparams.rgvarg = new VARIANT[dispparams.cArgs];
			dispparams.cNamedArgs = 0;

			for (int i = 0; i < arraySize; i++)
			{
				CComBSTR bstr = pArrFuncArgs->GetAt(arraySize - 1 - i); // back reading
				bstr.CopyTo(&dispparams.rgvarg[i].bstrVal);
				dispparams.rgvarg[i].vt = VT_BSTR;
			}

			EXCEPINFO excepInfo;
			memset(&excepInfo, 0, sizeof excepInfo);
			UINT nArgErr = (UINT)-1;  // initialize to invalid arg
			//Call JavaScript function         
			if (SUCCEEDED(spScript->Invoke(dispid, IID_NULL, 0, DISPATCH_METHOD, 
				&dispparams, &vaResult, &excepInfo, &nArgErr)))
			{
				//Done!
				bRes = TRUE;
			}

			//Free mem
			delete[] dispparams.rgvarg;
		}
	}

	if (pOutVarRes)
		*pOutVarRes = vaResult;

	return bRes;
}

void CHtmlCtrl::ExecuteScriptInAllFrames(
	const CString &sScript, const CString &sLanguage /*= _T("JavaScript")*/)
{
	CComPtr<IDispatch> spDisp = GetSafeHtmlDocument();
	if (NULL != spDisp)
	{
		CComPtr<IHTMLDocument2> doc;
		spDisp->QueryInterface(IID_IHTMLDocument2, reinterpret_cast<void**>(&doc));
		if (NULL != doc)
		{
			CScriptRunner runner(sScript, sLanguage);
			runner.Run(doc);
		}
	}
}

void CHtmlCtrl::ExecuteScriptInAllFrames(std::vector<std::pair<CString, CComVariant> >& vctRes, 
	const CString &sScript, const CString &sLanguage /*= _T("JavaScript")*/)
{
	CComPtr<IDispatch> spDisp = GetSafeHtmlDocument();
	if (NULL != spDisp)
	{
		CComPtr<IHTMLDocument2> doc;
		spDisp->QueryInterface(IID_IHTMLDocument2, reinterpret_cast<void**>(&doc));
		if (NULL != doc)
		{
			CScriptRunnerEx runner(sScript, sLanguage);
			runner.Run(doc);
			vctRes = runner.m_vctResults;
		}
	}
}

BOOL CHtmlCtrl::CallScriptFunction(LPCTSTR pStrFuncName, CStringArray* pArrFuncArgs, CComVariant* pOutVarRes)
{
	//Call client function in HTML
	//'pStrFuncName' = client script function name
	//'pArrFuncArgs' = if not NULL, list of arguments
	//'pOutVarRes' = if not NULL, will receive the return value
	//RETURN:
	//      = TRUE if done
	BOOL bRes = FALSE;

	CComVariant vaResult;

	CComPtr<IDispatch> spDisp = GetSafeHtmlDocument();
	if (NULL != spDisp)
	{
		CComPtr<IHTMLDocument2> doc;
		spDisp->QueryInterface(IID_IHTMLDocument2, reinterpret_cast<void**>(&doc));
		if (NULL != doc)
		{
			bRes = CallScriptFunction(doc, pStrFuncName, pArrFuncArgs, pOutVarRes);
		}
	}

	return bRes;
}

CString CHtmlCtrl::GetElementInputValue(const CString& idEle)
{
	std::vector<std::pair<CString, CComVariant> > vctRes;
	CString sHack;
	sHack.Format(_T("document.getElementById('%s').value"), idEle);
	ExecuteScriptInAllFrames(vctRes, sHack);
	for (int i = 0; i < vctRes.size(); ++i)
	{
		if (vctRes[i].second.vt == VT_BSTR && !CString(vctRes[i].second).IsEmpty())
		{
			return CString(vctRes[i].second.bstrVal);
		}
	}

	return _T("");
}

void CHtmlCtrl::SetElementInputValue(const CString& idEle, const CString& val)
{
	CString sHack;
	sHack.Format(_T("document.getElementById('%s').value='%s'"), idEle, val);
	ExecuteScriptInAllFrames(sHack);
}

//////////////////////////////////////遍历器//////////////////////////////////////

void CErgodicFrameHandler::Run(CComPtr<IHTMLDocument2> &parentDoc)
{
	if (parentDoc == NULL)
		return;
	
	//Touch Me
	OnFrame(parentDoc);

	CComPtr<IHTMLFramesCollection2> spFramesCol;
	HRESULT hr = parentDoc->get_frames(&spFramesCol);
	if (SUCCEEDED(hr) && spFramesCol != NULL)
	{
		long lSize = 0;
		hr = spFramesCol->get_length(&lSize);
		if (SUCCEEDED(hr))
		{
			for (int i = 0; i < lSize; i++)
			{
				VARIANT frameRequested;
				VARIANT frameOut;
				frameRequested.vt = VT_UI4;
				frameRequested.lVal = i;
				hr = spFramesCol->item(&frameRequested, &frameOut);
				if (SUCCEEDED(hr) && frameOut.pdispVal != NULL)
				{
					CComPtr<IHTMLWindow2> spChildWindow;
					hr = frameOut.pdispVal->QueryInterface(IID_IHTMLWindow2, reinterpret_cast<void**>(&spChildWindow));
					if (SUCCEEDED(hr) && spChildWindow != NULL)
					{
						CComPtr<IHTMLDocument2> spChildDocument;
						hr = spChildWindow->get_document(reinterpret_cast<IHTMLDocument2**>(&spChildDocument));
						if (SUCCEEDED(hr) && spChildDocument != NULL)
						{
							Run(spChildDocument);
						}
					}
					frameOut.pdispVal->Release();
				}
			}
		}
	}
}

CErgodicFrameHandler::CErgodicFrameHandler()
{

}

CErgodicFrameHandler::~CErgodicFrameHandler()
{

}

CScriptErrorHandler::CScriptErrorHandler()
{
	m_sScript =
		_T("function fnOnError(msg,url,lineno){")
#if _DEBUG
		_T("alert('script error:\\nURL:'+url")
		_T("+'\\nMSG:'+msg +'\\nLine:'+lineno+'\\nframes:' + window.frames.length);")
#endif
		_T("return true;}")
		_T("window.onerror=fnOnError;");
}

CScriptErrorHandler::~CScriptErrorHandler()
{
}

void CScriptErrorHandler::OnFrame(CComPtr<IHTMLDocument2> &doc)
{
	CHtmlCtrl::ExecuteScript(doc, m_sScript);
}

CScriptRunner::CScriptRunner(const CString& sScript, const CString& sScriptType/* = _T("JavaScript")*/)
	: m_sScript(sScript)
	, m_sScriptType(sScriptType)
{
}

CScriptRunner::~CScriptRunner()
{
}

void CScriptRunner::OnFrame(CComPtr<IHTMLDocument2> &doc)
{
	CHtmlCtrl::ExecuteScript(doc, m_sScript, m_sScriptType);
}

CScriptRunnerEx::CScriptRunnerEx(const CString& sScript, const CString& sScriptType /*= _T("JavaScript")*/)
	: m_sScript(sScript)
	, m_sScriptType(sScriptType)
{
}

CScriptRunnerEx::~CScriptRunnerEx()
{
}

void CScriptRunnerEx::OnFrame(CComPtr<IHTMLDocument2> &doc)
{
	CComVariant res;
	CHtmlCtrl::ExecuteScript(doc, res, m_sScript, m_sScriptType);
	BSTR bstrUrl;
	doc->get_URL(&bstrUrl);
	m_vctResults.push_back(std::make_pair(CString(bstrUrl), res));
}
