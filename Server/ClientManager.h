#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>
#include <QTime>
#include "Other.h"
#include "MessageConverter.h"

class ClientManager : public QObject
{
	Q_OBJECT

	enum ClientStatus
	{
		UnknownClientStatus = 0,
		ConnectedStatus,
		LoginedStatus
	};
	enum NatCheckStatus
	{
		UnknownNatCheckStatus = 0,
		Step1_1WaitingForClient1,
		Step1_12SendingToClient1,
		Step2_Type1_1SendingToClient12,
		Step2_Type2_2WaitingForClient1,
		NatCheckFinished
	};

	struct ClientInfo
	{
		ClientStatus status;
		QString identifier;
		QString userName;
		NatType natType = UnknownNatType;
		NatCheckStatus natStatus = UnknownNatCheckStatus;
		QHostAddress udpHostAddress;
		quint16 udp1Port1 = 0;
		quint16 udp1LocalPort = 0;
		quint16 udp2LocalPort = 0;
		QHostAddress localAddress;
		QString gatewayInfo;
		bool upnpAvailable = false;
		QTime lastInTime;
		QTime lastOutTime;
	};

	enum TunnelStatus
	{
		UnknownTunnelStatus = 0,
		ReadyTunnelingStatus,
		TunnelingStatus
	};

	struct TunnelInfo
	{
		TunnelStatus status = UnknownTunnelStatus;
		QString clientAUserName;		// AΪ���𷽣�BΪ���շ�����Ȼ�������֣��������϶Ե�
		QString clientBUserName;
		quint16 clientAUdp2UpnpPort = 0;	// ���������upnp������˿ںŷ�0
		quint16 clientBUdp2UpnpPort = 0;
	};

public:
	ClientManager(QObject *parent = 0);
	~ClientManager();

	void setGlobalKey(QByteArray key);
	void setUserCacheFileName(QString fileName);

	bool start(quint16 tcpPort, quint16 udpPort1, quint16 udpPort2);
	bool stop();

private slots:
	void onTcpNewConnection();
	void onTcpDisconnected();
	void onTcpReadyRead();
	void onUdp1ReadyRead();
	void onUdp2ReadyRead();
	void timerFunction300ms();
	void timerFunction15s();

private:
	QUdpSocket * getUdpServer(int index);

	bool loadUserCache();
	bool saveUserCache();

	bool checkStatus(QTcpSocket & tcpSocket, ClientInfo & client, ClientStatus correctStatus, NatCheckStatus correctNatStatus);
	bool checkStatus(QTcpSocket & tcpSocket, ClientInfo & client, ClientStatus correctStatus);
	bool checkStatusAndDisconnect(QTcpSocket & tcpSocket, ClientInfo & client, QString functionName, ClientStatus correctStatus, NatCheckStatus correctNatStatus);
	bool checkStatusAndDisconnect(QTcpSocket & tcpSocket, ClientInfo & client, QString functionName, ClientStatus correctStatus);

	void disconnectClient(QTcpSocket & tcpSocket, ClientInfo & client, QString reason);
	void sendTcp(QTcpSocket & tcpSocket, ClientInfo & client, QByteArray type, QByteArrayMap argument);
	void sendUdp(int index, QByteArray type, QByteArrayMap argument, QHostAddress hostAddress, quint16 port);
	void onUdpReadyRead(int index);

	QString getBoundUserName(QString identifier);
	QString getBoundIdentifier(QString userName);
	bool checkCanTunnel(ClientInfo & localClient, QString peerUserName, bool * outLocalNeedUpnp, bool * outPeerNeedUpnp, QString * outFailReason);
	bool isExistTunnel(QString userName1, QString userName2);
	// ����Client���ͺͿ��ܿ��õ�upnp�˿���ȷ���ⲿ���Ӷ˿ڣ�
	quint16 getExternalTunnelPort(ClientInfo & client, quint16 upnpPort);
	// ���ݺͶ��������/������������ȡClient���ŵ����Ӷ˿�
	void getBetterTunnelAddressPort(ClientInfo & client, ClientInfo & peerClient, quint16 clientUpnpPort, QHostAddress * outAddress, quint16 * outPort);
	int getNextTunnelId();

	bool getTcpSocketAndClientInfoByUserName(QString userName, QTcpSocket ** outTcpSocket, ClientInfo ** outClientInfo);
	TunnelInfo * getTunnelInfo(int tunnelId);

	void clearUserTunnel(QString userName, QString reason);

	void dealTcpIn(QByteArray line, QTcpSocket & tcpSocket, ClientInfo & client);
	void dealUdpIn(int index, const QByteArray & line, QHostAddress hostAddress, quint16 port);

	void tcpIn_heartbeat(QTcpSocket & tcpSocket, ClientInfo & client);
	void tcpOut_heartbeat(QTcpSocket & tcpSocket, ClientInfo & client);

	void tcpOut_hello(QTcpSocket & tcpSocket, ClientInfo & client);

	void tcpIn_login(QTcpSocket & tcpSocket, ClientInfo & client, QString identifier, QString userName);
	void tcpOut_login(QTcpSocket & tcpSocket, ClientInfo & client, bool loginOk, QString userName, QString msg, quint16 serverUdpPort1 = 0, quint16 serverUdpPort2 = 0);
	bool login(QTcpSocket & tcpSocket, ClientInfo & client, QString identifier, QString userName, QString * outMsg);

	void tcpIn_localNetwork(QTcpSocket & tcpSocket, ClientInfo & client, QHostAddress localAddress, quint16 clientUdp1LocalPort, QString gatewayInfo);

	void udpIn_checkNatStep1(int index, QTcpSocket & tcpSocket, ClientInfo & client, QHostAddress clientUdp1HostAddress, quint16 clientUdp1Port1);
	void tcpIn_checkNatStep1(QTcpSocket & tcpSocket, ClientInfo & client, int partlyType, quint16 clientUdp2LocalPort);

	void tcpIn_checkNatStep2Type1(QTcpSocket & tcpSocket, ClientInfo & client, NatType natType);
	void udpIn_checkNatStep2Type2(int index, QTcpSocket & tcpSocket, ClientInfo & client, quint16 clientUdp1Port2);
	void tcpOut_checkNatStep2Type2(QTcpSocket & tcpSocket, ClientInfo & client, NatType natType);

	void tcpIn_upnpAvailability(QTcpSocket & tcpSocket, ClientInfo & client, bool on);

	void udpIn_updateAddress(int index, QTcpSocket & tcpSocket, ClientInfo & client, QHostAddress clientUdp1HostAddress, quint16 clientUdp1Port1);

	void tcpIn_refreshOnlineUser(QTcpSocket & tcpSocket, ClientInfo & client);
	void tcpOut_refreshOnlineUser(QTcpSocket & tcpSocket, ClientInfo & client, QString onlineUser);

	void tcpIn_tryTunneling(QTcpSocket & tcpSocket, ClientInfo & client, QString peerUserName);
	void tcpOut_tryTunneling(QTcpSocket & tcpSocket, ClientInfo & client, QString peerUserName, bool canTunnel, bool needUpnp, QString failReason);

	void tcpIn_readyTunneling(QTcpSocket & tcpSocket, ClientInfo & client, QString peerUserName, QString peerLocalPassword, quint16 udp2UpnpPort, int requestId);
	void tcpOut_readyTunneling(QTcpSocket & tcpSocket, ClientInfo & client, int requestId, int tunnelId, QString peerUserName);

	void tcpOut_startTunneling(QTcpSocket & tcpSocket, ClientInfo & client, int tunnelId, QString localPassword, QString peerUserName, QHostAddress peerAddress, quint16 peerPort, bool needUpnp);
	void tcpIn_startTunneling(QTcpSocket & tcpSocket, ClientInfo & client, int tunnelId, bool canTunnel, quint16 udp2UpnpPort, QString errorString);

	void tcpOut_tunneling(QTcpSocket & tcpSocket, ClientInfo & client, int tunnelId, QHostAddress peerAddress, quint16 peerPort);

	void tcpIn_closeTunneling(QTcpSocket & tcpSocket, ClientInfo & client, int tunnelId, QString reason);
	void tcpOut_closeTunneling(QTcpSocket & tcpSocket, ClientInfo & client, int tunnelId, QString reason);

private:
	bool m_running = false;
	MessageConverter m_messageConverter;
	QTcpServer m_tcpServer;
	QUdpSocket m_udpServer1;
	QUdpSocket m_udpServer2;
	QString m_userCacheFileName;
	int m_lastTunnelId = 0;
	QMap<QString, QString> m_mapUserNameIdentifier;
	QMap<QTcpSocket*, ClientInfo> m_mapClientInfo;
	QMap<QString, QTcpSocket*> m_mapUserTcpSocket;
	QSet<QString> m_lstLoginedIdentifier;
	QMap<int, TunnelInfo> m_mapTunnelInfo;
	QTimer m_timer300ms;
	QTimer m_timer15s;
	QSet<QTcpSocket*> m_lstNeedSendUdp;
};
