// CTextSendEditor.cpp: 实现文件
//

#include "pch.h"
#include "NetDebugger.h"
#include "CTextSendEditor.h"
#include "TextEncodeType.h"
#include "UserWMDefine.h"

// CTextSendEditor
CTextSendEditor::CTextSendEditor(CWnd* pParentWnd) :
	m_ParentWnd(pParentWnd)//IDD_TEXT_SEND_EDITOR
{

}

CTextSendEditor::~CTextSendEditor()
{
}

IMPLEMENT_DYNAMIC(CTextSendEditor, ISendEditor)

BEGIN_MESSAGE_MAP(CTextSendEditor, CMFCPropertyPage)
	ON_MESSAGE(kWM_SELECT_BUTTON_CHANGED, &CTextSendEditor::OnEditDisplayTypeChanged)
	ON_BN_CLICKED(IDC_BUTTON_SEND_CLEAR, &CTextSendEditor::OnBnClickedButtonSendClear)
END_MESSAGE_MAP()

void CTextSendEditor::DoDataExchange(CDataExchange* pDX)
{
	CMFCPropertyPage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SEND_DISPLAY_TYPE, m_SendDisplayTypeCtrl);
	DDX_Control(pDX, IDC_SEND_EDITBOX, m_SendEditCtrl);
}

static void InitDisplayTypeCtrl(CSelectControl& ctrl)
{
	for (int i = 0; i < (int)TextEncodeType::_MAX; ++i)
	{
		ctrl.AddButton(TextEncodeTypeName((TextEncodeType)i), i);
	}
	ctrl.SetValue((UINT)TextEncodeType::ASCII);
}

BOOL CTextSendEditor::OnInitDialog(void)
{
	CMFCPropertyPage::OnInitDialog();
	InitDisplayTypeCtrl(m_SendDisplayTypeCtrl);
	m_SendEditCtrl.SetReadOnly(FALSE);
	m_SendDisplayTypeCtrl.SetValue(theApp.GetProfileInt(L"Setting", L"SendDisplayType", 0));
	OnEditDisplayTypeChanged(m_SendDisplayTypeCtrl.GetDlgCtrlID(), m_SendDisplayTypeCtrl.GetValue());

	SetDlgItemText(IDC_BUTTON_SEND_CLEAR, LSTEXT(MAINWND.BUTTON.SEND.CLEAR));
	return TRUE;
}

void CTextSendEditor::UpdateLanguage(void)
{
	SetDlgItemText(IDC_BUTTON_SEND_CLEAR, LSTEXT(MAINWND.BUTTON.SEND.CLEAR));
}

UINT CTextSendEditor::GetDataType(void)
{
	return m_SendDisplayTypeCtrl.GetValue();
}
void CTextSendEditor::SetDataType(UINT type)
{
	m_SendDisplayTypeCtrl.SetValue(type);
}


void CTextSendEditor::GetDataBuffer(std::function<bool(const uint8_t* buffer, size_t size)> handler)
{
	std::vector<uint8_t> buffer;
	buffer.reserve(1024);
	Transform::DecodeWStringTo(m_SendEditCtrl.GetContent(), (TextEncodeType)m_SendDisplayTypeCtrl.GetValue(), buffer);
	handler(buffer.data(), buffer.size());
}

void CTextSendEditor::ClearContent(void)
{
	m_SendEditCtrl.ClearAll();
}

void CTextSendEditor::SetContent(const void * data, size_t size)
{
	std::vector<uint8_t> buffer;
	buffer.resize(size);
	memcpy(buffer.data(), data, size);
	auto text = Transform::DecodeToWString(
		buffer,
		(TextEncodeType)GetDataType()
	);
	m_SendEditCtrl.SetText(text);
}

void CTextSendEditor::Create(void)
{
	CMFCPropertyPage::Create(IDD_TEXT_SEND_EDITOR, m_ParentWnd);
}

void CTextSendEditor::Destroy(void)
{
}


LRESULT CTextSendEditor::OnEditDisplayTypeChanged(WPARAM wParam, LPARAM lParam)
{
	auto id = static_cast<UINT>(wParam);
	auto value = static_cast<TextEncodeType>(lParam);
	if (id == IDC_SEND_DISPLAY_TYPE)
	{
		theApp.WriteProfileInt(L"Setting", L"SendDisplayType", m_SendDisplayTypeCtrl.GetValue());
		if (value == TextEncodeType::HEX)
		{
			m_SendEditCtrl.EnterHEXMode();
		}
		else
		{
			m_SendEditCtrl.ExitHEXMode();
		}
	}
	return 0;
}

void CTextSendEditor::OnBnClickedButtonSendClear()
{
	m_SendEditCtrl.ClearAll();
}


