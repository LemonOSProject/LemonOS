namespace Lemon::GUI{
	enum MsgBoxButtons{
		MsgButtonsOK,
		MsgButtonsOKCancel,
	};

	int DisplayMessageBox(const char* title, const char* message, MsgBoxButtons buttons = MsgButtonsOK);
}