// Altirra asynchronous native socket worker for POSIX platforms

#ifndef f_AT_ATNETWORKSOCKETS_INTERNAL_SOCKETWORKER_H
#define f_AT_ATNETWORKSOCKETS_INTERNAL_SOCKETWORKER_H

#include <bitset>
#include <deque>
#include <utility>
#include <vector>

#include <vd2/system/VDString.h>
#include <vd2/system/refcount.h>
#include <vd2/system/thread.h>
#include <at/atnetwork/socket.h>
#include <at/atnetworksockets/internal/socketutils.h>

class ATNetSocketWorker;
class IATAsyncDispatcher;

using ATSocketNativeHandle = int;
constexpr ATSocketNativeHandle kATInvalidSocket = -1;

struct ATNetSocketSyncContext final : public vdrefcount {
	VDCriticalSection mMutex;

	// Mutex used exclusively for callbacks. The main mutex must never be held
	// while locking this.
	VDCriticalSection mCallbackMutex;

	ATNetSocketWorker *mpWorker = nullptr;
};

class ATNetSocket : public vdrefcounted<IATSocket> {
public:
	ATNetSocket(ATNetSocketSyncContext& syncContext);
	~ATNetSocket();

	virtual void Shutdown();

	bool IsAbandoned() const;
	bool IsHardClosing_Locked() const;
	void QueueError_Locked(ATSocketError error);

	int Release() override;

	void SetOnEvent(IATAsyncDispatcher *dispatcher, vdfunction<void(const ATSocketStatus&)> fn, bool callIfReady) override;
	ATSocketStatus GetSocketStatus() const override;
	void CloseSocket(bool force) override;
	void PollSocket() override;

	virtual void Update_Locked() = 0;
	virtual short GetPollEvents_Locked() const = 0;
	virtual void HandlePollEvent_Locked(short events) = 0;

protected:
	friend class ATNetSocketWorker;

	void QueueEvent_Locked();
	void FlushEvent();
	void CloseSocket_Locked(bool force);
	void CloseNativeSocket_Locked();
	virtual ATSocketStatus GetSocketStatus_Locked() const = 0;

	vdrefptr<ATNetSocketSyncContext> mpSyncContext;

	enum class State {
		Created,
		Listen,
		Listening,
		Accept,
		Connect,
		Connecting,
		Connected,
		Close,
		Closing,
		Closed
	} mState = State::Created;

	ATSocketNativeHandle mSocketHandle = kATInvalidSocket;
	bool mbRequestedShutdownSend = false;
	bool mbRequestedShutdownRecv = false;
	bool mbSocketShutdownSend = false;
	bool mbSocketShutdownRecv = false;
	bool mbEventPending = false;
	ATSocketError mError = ATSocketError::None;
	int mSocketIndex = -1;

	IATAsyncDispatcher *mpOnEventDispatcher = nullptr;
	uint64 mOnEventToken = 0;
	vdfunction<void(const ATSocketStatus&)> mpOnEventFn;
};

template<typename T>
class ATNetSocketT : public ATNetSocket, public T {
public:
	using ATNetSocket::ATNetSocket;

	int AddRef() override { return ATNetSocket::AddRef(); }
	int Release() override { return ATNetSocket::Release(); }
	void CloseSocket(bool force) override { ATNetSocket::CloseSocket(force); }
	ATSocketStatus GetSocketStatus() const override { return ATNetSocket::GetSocketStatus(); }
	void SetOnEvent(IATAsyncDispatcher *dispatcher, vdfunction<void(const ATSocketStatus&)> fn, bool callIfReady) override {
		ATNetSocket::SetOnEvent(dispatcher, std::move(fn), callIfReady);
	}
	void PollSocket() override { ATNetSocket::PollSocket(); }
};

class ATNetStreamSocket final : public ATNetSocketT<IATStreamSocket> {
public:
	ATNetStreamSocket(ATNetSocketSyncContext& syncContext);
	ATNetStreamSocket(ATNetSocketSyncContext& syncContext, const ATSocketAddress& connectedAddress, ATSocketNativeHandle socket);
	~ATNetStreamSocket();

	void Connect(const ATSocketAddress& socketAddress, bool dualStack);
	ATSocketAddress GetLocalAddress() const override;
	ATSocketAddress GetRemoteAddress() const override;
	sint32 Recv(void *buf, uint32 len) override;
	sint32 Send(const void *buf, uint32 len) override;
	void ShutdownSocket(bool send, bool receive) override;

	void Update_Locked() override;
	short GetPollEvents_Locked() const override;
	void HandlePollEvent_Locked(short events) override;
	ATSocketStatus GetSocketStatus_Locked() const override;

private:
	bool InitSocket_Locked();
	void UpdateLocalAddress_Locked();
	void DoRead_Locked();
	void DoWrite_Locked();
	void DoClose_Locked();

	bool mbSocketRemoteClosed = false;
	bool mbDualStack = false;
	ATSocketAddress mLocalAddress;
	ATSocketAddress mConnectAddress;
	std::vector<char> mReadBuffer;
	std::vector<char> mWriteBuffer;
};

class ATNetListenSocket final : public ATNetSocketT<IATListenSocket> {
public:
	ATNetListenSocket(ATNetSocketSyncContext& syncContext, const ATSocketAddress& bindAddress, bool dualStack);
	~ATNetListenSocket();

	vdrefptr<IATStreamSocket> Accept() override;
	void Shutdown() override;
	void Update_Locked() override;
	short GetPollEvents_Locked() const override;
	void HandlePollEvent_Locked(short events) override;
	ATSocketStatus GetSocketStatus_Locked() const override;

private:
	void TryAccept_Locked();

	ATSocketAddress mBindAddress;
	ATSocketAddress mPendingAddress;
	ATSocketNativeHandle mPendingSocket = kATInvalidSocket;
	bool mbDualStack = false;
};

class ATNetDatagramSocket final : public ATNetSocketT<IATDatagramSocket> {
public:
	ATNetDatagramSocket(ATNetSocketSyncContext& syncContext, const ATSocketAddress& bindAddress, bool dualStack);
	~ATNetDatagramSocket();

	sint32 RecvFrom(ATSocketAddress& address, void *data, uint32 maxlen) override;
	bool SendTo(const ATSocketAddress& address, const void *data, uint32 len) override;

	void Update_Locked() override;
	short GetPollEvents_Locked() const override;
	void HandlePollEvent_Locked(short events) override;
	ATSocketStatus GetSocketStatus_Locked() const override;

private:
	static constexpr uint32 kMaxDatagramSize = 1536;
	static constexpr size_t kBufferSize = 4096;

	struct Packet {
		ATSocketAddress mAddress;
		std::vector<uint8> mData;
	};

	void DoRead_Locked();
	void DoWrite_Locked();
	void DoClose_Locked();

	ATSocketAddress mBindAddress;
	bool mbDualStack = false;
	std::deque<Packet> mReadQueue;
	std::deque<Packet> mWriteQueue;
	size_t mReadBytes = 0;
	size_t mWriteBytes = 0;
};

class ATNetSocketWorker final : public VDThread {
public:
	ATNetSocketWorker();
	~ATNetSocketWorker();

	bool Init();
	void Shutdown();

	vdrefptr<ATNetStreamSocket> CreateStreamSocket();
	vdrefptr<ATNetStreamSocket> CreateStreamSocket(const ATSocketAddress& connectedAddress, ATSocketNativeHandle socket);
	vdrefptr<ATNetListenSocket> CreateListenSocket(const ATSocketAddress& bindAddress, bool dualStack);
	vdrefptr<ATNetDatagramSocket> CreateDatagramSocket(const ATSocketAddress& bindAddress, bool dualStack);

	void RequestSocketUpdate_Locked(const ATNetSocket& socket);

private:
	static constexpr size_t kMaxSockets = 63;

	bool RegisterSocket_Locked(ATNetSocket& socket);
	void Wake();
	void ThreadRun() override;

	vdrefptr<ATNetSocketSyncContext> mpSyncContext;
	bool mbExitRequested = false;
	bool mbUpdateSockets = false;
	int mWakeReadHandle = -1;
	int mWakeWriteHandle = -1;
	uint8 mNumSockets = 0;
	vdrefptr<ATNetSocket> mSocketTable[kMaxSockets];
	std::bitset<kMaxSockets> mSocketsNeedUpdate;
};

#endif
