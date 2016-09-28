#include "MainDlg.h"
#include <QTcpServer>
#include <QSettings>
#include <QMessageBox>
#include <QInputDialog>
#include "Other.h"
#include "GuideDlg.h"

MainDlg::MainDlg(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);

	m_labelStatus = new QLabel(U16("δ����"));
	m_labelNatType = new QLabel(U16(" "));
	m_labelUpnp = new QLabel(U16(" "));

	m_labelStatus->setMinimumWidth(60);
	m_labelNatType->setMinimumWidth(60);
	m_labelUpnp->setMinimumWidth(60);

	ui.statusBar->addPermanentWidget(m_labelUpnp);
	ui.statusBar->addPermanentWidget(m_labelNatType);
	ui.statusBar->addPermanentWidget(m_labelStatus);

	m_tableModel = new QStandardItemModel(ui.tableView);
	ui.tableView->setModel(m_tableModel);
	m_tableModel->setHorizontalHeaderLabels(U16("tunnelId,�Է��û���,�Է�IP��ַ,״̬,����").split(",", QString::SkipEmptyParts));
	ui.tableView->setColumnHidden(0, true);
	ui.tableView->setColumnHidden(2, true);
	ui.tableView->setColumnWidth(1, 100);
	ui.tableView->setColumnWidth(2, 100);
	ui.tableView->setColumnWidth(3, 100);
	ui.tableView->setColumnWidth(4, 160);

	ui.btnRefreshOnlineUser->setIcon(QIcon(":/MainDlg/refresh.png"));

	connect(ui.editLocalPassword, SIGNAL(textChanged(const QString &)), this, SLOT(onEditLocalPasswordChanged()));
	connect(ui.btnRefreshOnlineUser, SIGNAL(clicked()), this, SLOT(onBtnRefreshOnlineUser()));
	connect(ui.btnTunnel, SIGNAL(clicked()), this, SLOT(onBtnTunnel()));

	leadWindowsFirewallPopup();

	start();
}

MainDlg::~MainDlg()
{
	stop();
}

void MainDlg::start()
{
	if (m_workingThread.isRunning())
		return;

	m_workingThread.start();

	m_client = new Client();
	m_transferManager = new TransferManager(nullptr, m_client);

	m_client->moveToThread(&m_workingThread);
	m_transferManager->moveToThread(&m_workingThread);

	connect(m_client, SIGNAL(connected()), this, SLOT(onConnected()));
	connect(m_client, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
	connect(m_client, SIGNAL(logined()), this, SLOT(onLogined()));
	connect(m_client, SIGNAL(loginFailed(QString, QString)), this, SLOT(onLoginFailed(QString, QString)));
	connect(m_client, SIGNAL(natTypeConfirmed(NatType)), this, SLOT(onNatTypeConfirmed(NatType)));
	connect(m_client, SIGNAL(upnpStatusChanged(UpnpStatus)), this, SLOT(onClientUpnpStatusChanged(UpnpStatus)));
	connect(m_client, SIGNAL(warning(QString)), this, SLOT(onClientWarning(QString)));

	connect(m_client, SIGNAL(replyRefreshOnlineUser(QStringList)), this, SLOT(onReplyRefreshOnlineUser(QStringList)));
	connect(m_client, SIGNAL(replyTryTunneling(QString, bool, bool, QString)), this, SLOT(onReplyTryTunneling(QString, bool, bool, QString)));
	connect(m_client, SIGNAL(replyReadyTunneling(int, int, QString)), this, SLOT(onReplyReadyTunneling(int, int, QString)));
	connect(m_client, SIGNAL(tunnelStarted(int, QString, QHostAddress)), this, SLOT(onTunnelStarted(int, QString, QHostAddress)));
	connect(m_client, SIGNAL(tunnelHandShaked(int)), this, SLOT(onTunnelHandShaked(int)));
	connect(m_client, SIGNAL(tunnelClosed(int,QString,QString)), this, SLOT(onTunnelClosed(int,QString,QString)));

	QSettings setting("NatTunnelClient.ini", QSettings::IniFormat);

	const QHostAddress serverAddress = QHostAddress(setting.value("Server/Address").toString());
	const int serverPort = setting.value("Server/Port").toInt();
	const QByteArray serverKey = setting.value("Server/Key").toByteArray();

	const QString userName = setting.value("Client/UserName").toString();
	const QString localPassword = setting.value("Client/LocalPassword", QString::number(rand_u32())).toString();
	QString randomIdentifierSuffix = setting.value("Client/RandomIdentifierSuffix").toString();
	if (randomIdentifierSuffix.isEmpty())
	{
		randomIdentifierSuffix = QString::number(rand_u32());
		setting.setValue("Client/RandomIdentifierSuffix", randomIdentifierSuffix);
	}


	if(setting.value("Other/ShowTunnelId").toInt() == 1)
		ui.tableView->setColumnHidden(0, false);
	if (setting.value("Other/ShowAddress").toInt() == 1)
		ui.tableView->setColumnHidden(2, false);

	setUserNameList(setting.value("Cache/UserNameList").toStringList());

	ui.editUserName->setText(userName);
	ui.editLocalPassword->setText(localPassword);

	m_client->setRandomIdentifierSuffix(randomIdentifierSuffix);
	m_client->setUserName(userName);
	m_client->setGlobalKey(serverKey);
	m_client->setServerInfo(serverAddress, serverPort);
	QMetaObject::invokeMethod(m_client, "start");
}

void MainDlg::stop()
{
	if (!m_workingThread.isRunning())
		return;

	m_client->deleteLater();
	m_transferManager->deleteLater();

	m_client = nullptr;
	m_transferManager = nullptr;

	m_workingThread.quit();
	m_workingThread.wait();
}


void MainDlg::leadWindowsFirewallPopup()
{
#if defined(Q_OS_WIN)
	// ����һ��TcpServer������Windows����ǽ�ᵯ����ʾ�������û�������������
	// ����������������Udp�ղ���δ������ַ���������ݰ�
	static bool invoked = false;
	if (invoked)
		return;
	QTcpServer tcpServer;
	tcpServer.listen(QHostAddress::Any, 0);
	invoked = true;
#endif
}

void MainDlg::closeEvent(QCloseEvent *event)
{
	QSettings setting("NatTunnelClient.ini", QSettings::IniFormat);
	setting.setValue("Client/LocalPassword", ui.editLocalPassword->text());
	setting.setValue("Cache/UserNameList", getUserNameList());
}
void MainDlg::onConnected()
{
	m_labelStatus->setText(U16("���ڵ�¼"));
}

void MainDlg::onDisconnected()
{	
	m_labelStatus->setText(U16("�Ͽ�"));
	m_labelNatType->clear();
	m_labelUpnp->clear();
	ui.btnTunnel->setEnabled(false);
	m_tableModel->removeRows(0, m_tableModel->rowCount());
}

void MainDlg::onLogined()
{
	onBtnRefreshOnlineUser();
	m_labelStatus->setText(U16("��¼�ɹ�"));
	m_labelNatType->setText(U16("���ڼ��NAT����"));
}

void MainDlg::onLoginFailed(QString userName, QString msg)
{
	QString tipText;
	if (userName.isEmpty())
		tipText = U16("��дһ���û���");
	else
		tipText = U16("%1��¼ʧ�ܣ�%2����дһ�����û���").arg(userName).arg(msg);
	const QString newUserName = QInputDialog::getText(this, U16("��д�û���"), tipText);
	if (newUserName.isNull())
	{
		this->close();
		return;
	}

	ui.editUserName->setText(newUserName);
	QSettings setting("NatTunnelClient.ini", QSettings::IniFormat);
	setting.setValue("Client/UserName", newUserName);

	QMetaObject::invokeMethod(m_client, "setUserName", Q_ARG(QString, newUserName));
	QMetaObject::invokeMethod(m_client, "tryLogin");
}

void MainDlg::onNatTypeConfirmed(NatType natType)
{
	m_labelNatType->setText(getNatDescription(natType));
	ui.btnTunnel->setEnabled(true);
}

void MainDlg::onClientUpnpStatusChanged(UpnpStatus upnpStatus)
{
	QString text;
	switch (upnpStatus)
	{
	case UpnpUnknownStatus:
		text = U16("Upnpδ֪״̬");
		break;
	case UpnpDiscovering:
		text = U16("���ڼ��upnp֧��");
		break;
	case UpnpUnneeded:
		text = U16("��ǰ���绷������Upnp");
		break;
	case UpnpQueryingExternalAddress:
		text = U16("Upnp���ڲ�ѯ������ַ");
		break;
	case UpnpOk:
		text = U16("Upnp����");
		break;
	case UpnpFailed:
		text = U16("Upnp������");
		break;
	default:
		break;
	}
	m_labelUpnp->setText(text);
}

void MainDlg::onClientWarning(QString msg)
{
	ui.statusBar->showMessage(msg);
}

void MainDlg::onEditLocalPasswordChanged()
{
	QMetaObject::invokeMethod(m_client, "setLocalPassword", Q_ARG(QString, ui.editLocalPassword->text()));
}

void MainDlg::onBtnRefreshOnlineUser()
{
	QMetaObject::invokeMethod(m_client, "refreshOnlineUser");
	ui.btnRefreshOnlineUser->setEnabled(false);
}

void MainDlg::onReplyRefreshOnlineUser(QStringList onlineUserList)
{
	const QString currentText = ui.comboBoxPeerUserName->currentText();
	ui.comboBoxPeerUserName->clear();
	ui.comboBoxPeerUserName->addItems(onlineUserList);
	ui.comboBoxPeerUserName->setCurrentText(currentText);
	ui.btnRefreshOnlineUser->setEnabled(true);
}

void MainDlg::onBtnTunnel()
{
	const QString peerUserName = ui.comboBoxPeerUserName->currentText();
	if (peerUserName.isEmpty())
		return;

	QMetaObject::invokeMethod(m_client, "tryTunneling", Q_ARG(QString, peerUserName));
	ui.btnTunnel->setEnabled(false);
}

void MainDlg::onReplyTryTunneling(QString peerUserName, bool canTunnel, bool needUpnp, QString failReason)
{
	if (!canTunnel)
	{
		QMessageBox::warning(this, U16("����ʧ��"), failReason);
		ui.btnTunnel->setEnabled(true);
		return;
	}

	insertTopUserName(peerUserName);
	const QString peerLocalPassword = QInputDialog::getText(this, U16("����"), U16("���� %1 �ı�������").arg(peerUserName), QLineEdit::Password);
	if (peerLocalPassword.isNull())
	{
		ui.btnTunnel->setEnabled(true);
		return;
	}

	QMetaObject::invokeMethod(m_client, "readyTunneling",
		Q_ARG(QString, peerUserName), Q_ARG(QString, peerLocalPassword), Q_ARG(bool, needUpnp));
}

void MainDlg::onReplyReadyTunneling(int requestId, int tunnelId, QString peerUserName)
{
	if (tunnelId == 0)
	{
		QMessageBox::warning(this, U16("����ʧ��"), U16("����ʧ��"));
		return;
	}

	ui.btnTunnel->setEnabled(true);
	updateTableRow(tunnelId, peerUserName, "", U16("׼����"));
}

void MainDlg::onTunnelStarted(int tunnelId, QString peerUserName, QHostAddress peerAddress)
{
	updateTableRow(tunnelId, peerUserName, peerAddress.toString(), U16("��ʼ����"));
	ui.statusBar->showMessage("");
}

void MainDlg::onTunnelHandShaked(int tunnelId)
{
	updateTableRow(tunnelId, QString(), QString(), U16("���ӳɹ�"));
}

void MainDlg::onTunnelClosed(int tunnelId, QString peerUserName, QString reason)
{
	deleteTableRow(tunnelId);
	ui.statusBar->showMessage(peerUserName + U16(" ���ӶϿ� ") + reason);
}

void MainDlg::onBtnCloseTunneling()
{
	QPushButton * btnCloseTunneling = (QPushButton*)sender();
	if (!btnCloseTunneling)
		return;
	const int tunnelId = btnCloseTunneling->property("tunnelId").toInt();
	if (tunnelId == 0)
		return;
	QMetaObject::invokeMethod(m_client, "closeTunneling", Q_ARG(int, tunnelId));
	btnCloseTunneling->setEnabled(false);
}

void MainDlg::onBtnAddTransfer()
{
	QPushButton * btnAddTransfer = (QPushButton*)sender();
	if (!btnAddTransfer)
		return;
	const int tunnelId = btnAddTransfer->property("tunnelId").toInt();
	if (tunnelId == 0)
		return;

	QString originalText;
	QString inputText = QInputDialog::getMultiLineText(this, U16("���ת��"), U16("ÿ��һ������ʽ��[���ض˿ں�] [Զ�̶˿ں�] [Զ��IP��ַ](������Ĭ��Ϊ127.0.0.1)"), originalText);
	if (inputText.isNull())
		return;

	QString errorMsg;
	QList<TransferInfo> lstTransferInfo = parseTransferInfoList(inputText, &errorMsg);
	if (errorMsg.length() > 0)
	{
		QMessageBox::warning(this, U16("���ת��"), errorMsg);
		return;
	}

	if (lstTransferInfo.isEmpty())
	{
		return;
	}

	QList<TransferInfo> lstFailed;
	QMetaObject::invokeMethod(m_transferManager, "addTransfer", Qt::BlockingQueuedConnection,
		Q_ARG(int, tunnelId), Q_ARG(QList<TransferInfo>, lstTransferInfo), Q_ARG(QList<TransferInfo>*, &lstFailed));

	if (lstFailed.size() > 0)
	{
		QStringList lineList;
		for (TransferInfo & transferInfo : lstFailed)
			lineList << QString("%1 %2 %3").arg(transferInfo.localPort).arg(transferInfo.remoteAddress.toString()).arg(transferInfo.remotePort);

		QMessageBox::warning(this, U16("���ת��"), lineList.join("\n") + U16("\n%1�����ʧ��").arg(lstFailed.size()));
	}

}

QList<TransferInfo> MainDlg::parseTransferInfoList(QString text, QString * outErrorMsg)
{
	QString dummy;
	if (!outErrorMsg)
		outErrorMsg = &dummy;
	outErrorMsg->clear();
	QList<TransferInfo> result;
	const QStringList lineList = text.split("\n", QString::KeepEmptyParts);
	for (int i = 0; i < lineList.size(); ++i)
	{
		QString line = lineList.at(i);
		line.trimmed();
		if (line.isEmpty())
			continue;

		QStringList fieldList = line.split(" ", QString::SkipEmptyParts);
		if (fieldList.size() != 2 && fieldList.size() != 3)
		{
			*outErrorMsg = U16("��%1�� ��Ч�ĸ�ʽ��'%2'").arg(i + 1).arg(line);
			return QList<TransferInfo>();
		}
		const QString localPortText = fieldList[0];
		const QString remotePortText = fieldList[1];
		const QString remoteAddressText = fieldList.size() >= 3 ? fieldList[2] : QString("127.0.0.1");

		bool localPortOk = false, remotePortOk = false;
		const int localPort = localPortText.toInt(&localPortOk);
		const QHostAddress remoteAddress = QHostAddress(remoteAddressText);
		const int remotePort = remotePortText.toInt(&remotePortOk);

		if (!localPortOk || localPort <= 0 || localPort > 65535)
		{
			*outErrorMsg = U16("��%1�� ��Ч�ı��ض˿ں� '%2'��").arg(i + 1).arg(localPortText);
			return QList<TransferInfo>();
		}
		if (!remotePortOk || remotePort <= 0 || remotePort > 65535)
		{
			*outErrorMsg = U16("��%1�� ��Ч��Զ�̶˿ں� '%2'��").arg(i + 1).arg(remotePortText);
			return QList<TransferInfo>();
		}
		if (remoteAddressText.isNull())
		{
			*outErrorMsg = U16("��%1�� ��Ч��Զ��IP��ַ '%2'��").arg(i + 1).arg(remoteAddressText);
			return QList<TransferInfo>();
		}

		TransferInfo transferInfo;
		transferInfo.localPort = localPort;
		transferInfo.remoteAddress = remoteAddress;
		transferInfo.remotePort = remotePort;

		result << transferInfo;
	}
	return result;
}

void MainDlg::updateTableRow(int tunnelId, QString peerUsername, QString peerAddress, QString status)
{
	const QString key = QString::number(tunnelId);
	QList<QStandardItem*> lstItem = m_tableModel->findItems(key);
	if (lstItem.isEmpty())
	{
		lstItem << new QStandardItem(key) << new QStandardItem(peerUsername) << new QStandardItem(peerAddress)
			<< new QStandardItem(status) << new QStandardItem();
		m_tableModel->appendRow(lstItem);

		QPushButton * btnCloseTunneling = new QPushButton(U16("�Ͽ�"));
		QPushButton * btnAddTransfer = new QPushButton(U16("���ת��"));
		btnCloseTunneling->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
		btnCloseTunneling->setProperty("tunnelId", tunnelId);
		btnAddTransfer->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
		btnAddTransfer->setProperty("tunnelId", tunnelId);

		connect(btnCloseTunneling, SIGNAL(clicked()), this, SLOT(onBtnCloseTunneling()));
		connect(btnAddTransfer, SIGNAL(clicked()), this, SLOT(onBtnAddTransfer()));

		QHBoxLayout * horizontalLayout = new QHBoxLayout();
		QWidget * containerWidget = new QWidget();
		containerWidget->setLayout(horizontalLayout);
		horizontalLayout->setMargin(0);
		horizontalLayout->setSpacing(0);
		horizontalLayout->addWidget(btnCloseTunneling);
		horizontalLayout->addWidget(btnAddTransfer);

		ui.tableView->setIndexWidget(m_tableModel->index(m_tableModel->rowCount() - 1, m_tableModel->columnCount() - 1), containerWidget);
	}
	else
	{
		const int row = lstItem.at(0)->row();
		if (!peerUsername.isNull())
			m_tableModel->item(row, 1)->setText(peerUsername);
		if (!peerAddress.isNull())
			m_tableModel->item(row, 2)->setText(peerAddress);
		if (!status.isNull())
			m_tableModel->item(row, 3)->setText(status);
	}
}

void MainDlg::deleteTableRow(int tunnelId)
{
	const QString key = QString::number(tunnelId);
	QList<QStandardItem*> lstItem = m_tableModel->findItems(key);
	if (lstItem.isEmpty())
		return;
	m_tableModel->removeRow(lstItem.at(0)->row());
}

void MainDlg::insertTopUserName(QString userName)
{
	bool noChange = false;
	for (int i = 0; i < ui.comboBoxPeerUserName->count(); ++i)
	{
		if (ui.comboBoxPeerUserName->itemText(i) == userName)
		{
			if (i > 0)
				ui.comboBoxPeerUserName->removeItem(i);
			else
				noChange = true;
			break;
		}
	}
	if (noChange)
		return;
	if (ui.comboBoxPeerUserName->count() >= 10)
		ui.comboBoxPeerUserName->removeItem(ui.comboBoxPeerUserName->count() - 1);
	ui.comboBoxPeerUserName->insertItem(0, userName);
	ui.comboBoxPeerUserName->setCurrentIndex(0);
}

QStringList MainDlg::getUserNameList()
{
	QStringList result;
	for (int i = 0; i < ui.comboBoxPeerUserName->count(); ++i)
		result << ui.comboBoxPeerUserName->itemText(i);
	return result;
}

void MainDlg::setUserNameList(QStringList userNameList)
{
	ui.comboBoxPeerUserName->clear();
	for (QString userName : userNameList)
		ui.comboBoxPeerUserName->addItem(userName);
}
