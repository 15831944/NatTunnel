#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTcpServer>
#include <QTimer>
#include <QTime>
#include "Peer.h"

class TcpTransfer : public QObject
{
	Q_OBJECT

private:
	enum FrameType
	{
		BeginUnknownFrameType = 0,
		HeartBeatType,
		AddTransferType,
		DeleteTransferType,
		NewConnectionType,
		DisconnectConnectionType,
		DataStreamType,
		AckType,
		EndUnknownFrameType,
	};

	struct SocketInInfo
	{
		qint64 peerSocketDescriptor = 0;		// �����socketDescriptor
		QTcpSocket * obj = nullptr;
		QByteArray cachedData;
		int peerWaitingSize = 0;				// �ȴ�����д����ֽ���
	};
	struct SocketOutInfo
	{
		qint64 socketDescriptor = 0;
		QTcpSocket * obj = nullptr;
		int peerWaitingSize = 0;
	};

	struct SocketIdentifier
	{
		union
		{
			qint64 socketDescriptor;
			qint64 peerSocketDescriptor;
		};
		quint8 direction;
		SocketIdentifier(qint64 socketDescriptor, quint8 direction)
		{
			this->socketDescriptor = socketDescriptor;
			this->direction = direction;
		}
		bool operator == (const SocketIdentifier & other)
		{
			return (this->socketDescriptor == other.socketDescriptor) && (this->direction == other.direction);
		}
	};

signals:
	void dataOutput(QByteArray package);
	bool isTunnelBusy();

public:
	TcpTransfer(QObject *parent = 0);
	~TcpTransfer();

	void dataInput(QByteArray package);

	bool addTransfer(quint16 localPort, quint16 remoteDestPort, QHostAddress remoteDestAddress);
	bool deleteTransfer(quint16 localPort);

private:
	static bool isValidFrameType(FrameType frameType);

private:
	SocketInInfo * findSocketIn(const qint64 & peerSocketDescriptor);
	SocketOutInfo * findSocketOut(const qint64 & socketDescriptor);

private:
	void dealFrame(FrameType type, const QByteArray & frameData);
	void input_heartBeat();
	void input_AddTransfer(quint16 localPort, quint16 remoteDestPort, QString remoteDestAddressText);
	void input_DeleteTransfer(quint16 localPort);
	void input_NewConnection(quint16 localPort, qint64 socketDescriptor);
	void input_DisconnectConnection(qint64 socketDescriptor, quint8 direction);
	void input_DataStream(qint64 socketDescriptor, quint8 direction, const char * data, int dataSize);
	void input_Ack(qint64 socketDescriptor, quint8 direction, int writtenSize);

private:
	void outputFrame(FrameType type, const QByteArray & frameData, const char * extraData = nullptr, int extraDataSize = 0);
	void output_heartBeat();
	void output_AddTransfer(quint16 localPort, quint16 remoteDestPort, QString remoteDestAddressText);
	void output_DeleteTransfer(quint16 localPort);
	void output_NewConnection(quint16 localPort, qint64 socketDescriptor);
	void output_DisconnectConnection(qint64 socketDescriptor, quint8 direction);
	void output_DataStream(qint64 socketDescriptor, quint8 direction, const char * data, int dataSize);
	void output_Ack(qint64 socketDescriptor, quint8 direction, int writtenSize);

private:
	int readAndSendSocketOut(SocketOutInfo & socketOut);
	int readAndSendSocketIn(SocketInInfo & socketIn);

private slots:
	void timerFunction15s();
	void onSocketInStateChanged(QAbstractSocket::SocketState state);
	void onSocketOutStateChanged(QAbstractSocket::SocketState state);
	void onTcpNewConnection();
	void onSocketOutReadyRead();
	void onSocketInReadyRead();
	void onSocketOutBytesWritten(qint64 size);
	void onSocketInBytesWritten(qint64 size);

private:
	QByteArray m_buffer;
	QMap<quint16, QTcpServer*> m_mapTcpServer;				// <localPort,...> ת���������
	QMap<quint16, Peer> m_mapTransferOut;					// <localPort,...> ת��������ض˿�-�����ַ�˿�
	QMap<quint16, Peer> m_mapTransferIn;					// <localPort,...> ת���������˿�-���ص�ַ�˿�
	QMap<qint64, SocketOutInfo> m_mapSocketOut;				// <peerSocketDescriptor,...> ת�����������
	QMap<qint64, SocketInInfo> m_mapSocketIn;				// <socketDescriptor,...> ת�����������
	QList<SocketIdentifier> m_lstGlobalWaitingSocket;		// ����ȫ�����ر��ȵȴ���Socket
	int m_globalWaitingSize = 0;							// ȫ�����صȴ��ֽ�����DataStreamTypeʵ���ֽ�����������Frame
	QTime m_lastOutTime;
	QTimer m_timer15s;
};
