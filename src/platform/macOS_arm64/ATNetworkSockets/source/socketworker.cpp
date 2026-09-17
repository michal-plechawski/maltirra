// Altirra asynchronous native socket worker for POSIX platforms

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <utility>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <at/atcore/asyncdispatcher.h>
#include <at/atnetworksockets/internal/socketworker.h>
#include <at/atnetworksockets/socketutils_posix.h>

namespace {
	constexpr size_t kStreamBufferSize = 4096;

	bool ATSetSocketNonblocking(int socketHandle) {
		const int flags = fcntl(socketHandle, F_GETFL, 0);
		if (flags < 0 || fcntl(socketHandle, F_SETFL, flags | O_NONBLOCK) < 0)
			return false;

		const int fdFlags = fcntl(socketHandle, F_GETFD, 0);
		if (fdFlags >= 0)
			fcntl(socketHandle, F_SETFD, fdFlags | FD_CLOEXEC);

#ifdef SO_NOSIGPIPE
		const int enabled = 1;
		setsockopt(socketHandle, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof enabled);
#endif
		return true;
	}

	int ATGetSocketError(int socketHandle) {
		int error = 0;
		socklen_t errorLen = sizeof error;
		if (getsockopt(socketHandle, SOL_SOCKET, SO_ERROR, &error, &errorLen) < 0)
			return errno;

		return error;
	}

	bool ATWouldBlock(int error) {
		return error == EAGAIN || error == EWOULDBLOCK;
	}
}

////////////////////////////////////////////////////////////////////////////////

ATNetSocket::ATNetSocket(ATNetSocketSyncContext& syncContext)
	: mpSyncContext(&syncContext)
{
}

ATNetSocket::~ATNetSocket() {
}

void ATNetSocket::Shutdown() {
	vdfunction<void(const ATSocketStatus&)> oldFn;

	vdsynchronized(mpSyncContext->mCallbackMutex) {
		if (mpOnEventDispatcher) {
			mpOnEventDispatcher->Cancel(&mOnEventToken);
			mpOnEventDispatcher = nullptr;
		}

		oldFn = std::move(mpOnEventFn);
	}

	vdsynchronized(mpSyncContext->mMutex) {
		CloseNativeSocket_Locked();
		mState = State::Closed;
		mbEventPending = false;
	}
}

bool ATNetSocket::IsAbandoned() const {
	return mRefCount == 1;
}

bool ATNetSocket::IsHardClosing_Locked() const {
	return mState == State::Close;
}

int ATNetSocket::Release() {
	const int rc = vdrefcounted::Release();

	if (rc == 1) {
		vdsynchronized(mpSyncContext->mMutex) {
			if (mSocketIndex >= 0 && mpSyncContext->mpWorker)
				mpSyncContext->mpWorker->RequestSocketUpdate_Locked(*this);
		}
	}

	return rc;
}

void ATNetSocket::QueueError_Locked(ATSocketError error) {
	if (mError == ATSocketError::None)
		mError = error;

	QueueEvent_Locked();
	CloseSocket_Locked(true);
}

void ATNetSocket::SetOnEvent(IATAsyncDispatcher *dispatcher, vdfunction<void(const ATSocketStatus&)> fn, bool callIfReady) {
	vdfunction<void(const ATSocketStatus&)> oldFn;
	const bool haveFn = fn != nullptr;

	vdsynchronized(mpSyncContext->mCallbackMutex) {
		if (mpOnEventDispatcher) {
			mpOnEventDispatcher->Cancel(&mOnEventToken);
			mpOnEventDispatcher = nullptr;
		}

		oldFn = std::move(mpOnEventFn);
		mpOnEventDispatcher = dispatcher;
		mpOnEventFn = std::move(fn);
	}

	oldFn = nullptr;

	if (callIfReady && haveFn) {
		const ATSocketStatus status = GetSocketStatus();

		vdsynchronized(mpSyncContext->mCallbackMutex) {
			if (mpOnEventFn)
				mpOnEventFn(status);
		}
	}
}

ATSocketStatus ATNetSocket::GetSocketStatus() const {
	vdsynchronized(mpSyncContext->mMutex) {
		return GetSocketStatus_Locked();
	}
}

void ATNetSocket::CloseSocket(bool force) {
	vdsynchronized(mpSyncContext->mMutex) {
		CloseSocket_Locked(force);
	}
}

void ATNetSocket::PollSocket() {
	vdsynchronized(mpSyncContext->mMutex) {
		QueueEvent_Locked();
	}
	FlushEvent();
}

void ATNetSocket::QueueEvent_Locked() {
	mbEventPending = true;
}

void ATNetSocket::FlushEvent() {
	ATSocketStatus status {};

	vdsynchronized(mpSyncContext->mMutex) {
		if (!mbEventPending)
			return;

		mbEventPending = false;
		status = GetSocketStatus_Locked();
	}

	vdsynchronized(mpSyncContext->mCallbackMutex) {
		if (mpOnEventFn) {
			if (mpOnEventDispatcher) {
				mpOnEventDispatcher->Queue(&mOnEventToken,
					[self = vdrefptr(this), status]() {
						vdsynchronized(self->mpSyncContext->mCallbackMutex) {
							if (self->mpOnEventFn)
								self->mpOnEventFn(status);
						}
					}
				);
			} else {
				mpOnEventFn(status);
			}
		}
	}
}

void ATNetSocket::CloseSocket_Locked(bool force) {
	if (mState == State::Closed || mState == State::Close)
		return;

	if (force) {
		mState = State::Close;
	} else if (mState != State::Closing) {
		mState = State::Closing;
		mbRequestedShutdownRecv = true;
	}

	if (mpSyncContext->mpWorker)
		mpSyncContext->mpWorker->RequestSocketUpdate_Locked(*this);
}

void ATNetSocket::CloseNativeSocket_Locked() {
	if (mSocketHandle != kATInvalidSocket) {
		close(mSocketHandle);
		mSocketHandle = kATInvalidSocket;
	}
}

////////////////////////////////////////////////////////////////////////////////

ATNetStreamSocket::ATNetStreamSocket(ATNetSocketSyncContext& syncContext)
	: ATNetSocketT(syncContext)
{
	mReadBuffer.reserve(kStreamBufferSize);
	mWriteBuffer.reserve(kStreamBufferSize);
}

ATNetStreamSocket::ATNetStreamSocket(ATNetSocketSyncContext& syncContext, const ATSocketAddress& connectedAddress, ATSocketNativeHandle socket)
	: ATNetStreamSocket(syncContext)
{
	mConnectAddress = connectedAddress;
	mSocketHandle = socket;
	mState = State::Accept;
}

ATNetStreamSocket::~ATNetStreamSocket() {
}

void ATNetStreamSocket::Connect(const ATSocketAddress& socketAddress, bool dualStack) {
	vdsynchronized(mpSyncContext->mMutex) {
		if (mState == State::Created) {
			mState = State::Connect;
			mbDualStack = dualStack;
			mConnectAddress = socketAddress;

			if (mpSyncContext->mpWorker)
				mpSyncContext->mpWorker->RequestSocketUpdate_Locked(*this);
		}
	}
}

ATSocketAddress ATNetStreamSocket::GetLocalAddress() const {
	vdsynchronized(mpSyncContext->mMutex) {
		return mLocalAddress;
	}
}

ATSocketAddress ATNetStreamSocket::GetRemoteAddress() const {
	vdsynchronized(mpSyncContext->mMutex) {
		return mConnectAddress;
	}
}

sint32 ATNetStreamSocket::Recv(void *buf, uint32 len) {
	if (!len)
		return 0;

	vdsynchronized(mpSyncContext->mMutex) {
		if (mState == State::Connect)
			return 0;

		if (mState != State::Connecting && mState != State::Connected && mState != State::Closing)
			return -1;

		const size_t actual = std::min<size_t>(len, mReadBuffer.size());
		if (actual) {
			memcpy(buf, mReadBuffer.data(), actual);
			mReadBuffer.erase(mReadBuffer.begin(), mReadBuffer.begin() + actual);

			if (mpSyncContext->mpWorker)
				mpSyncContext->mpWorker->RequestSocketUpdate_Locked(*this);
		}

		return (sint32)actual;
	}
}

sint32 ATNetStreamSocket::Send(const void *buf, uint32 len) {
	if (!len)
		return 0;

	vdsynchronized(mpSyncContext->mMutex) {
		if (mState != State::Connecting && mState != State::Connected && mState != State::Closing)
			return -1;

		const size_t actual = std::min<size_t>(len, kStreamBufferSize - mWriteBuffer.size());
		const char *src = static_cast<const char *>(buf);
		mWriteBuffer.insert(mWriteBuffer.end(), src, src + actual);

		if (actual && mpSyncContext->mpWorker)
			mpSyncContext->mpWorker->RequestSocketUpdate_Locked(*this);

		return (sint32)actual;
	}
}

void ATNetStreamSocket::ShutdownSocket(bool send, bool receive) {
	vdsynchronized(mpSyncContext->mMutex) {
		bool changed = false;

		if (send && !mbRequestedShutdownSend) {
			mbRequestedShutdownSend = true;
			changed = true;
		}

		if (receive && !mbRequestedShutdownRecv) {
			mbRequestedShutdownRecv = true;
			changed = true;
		}

		if (changed && mpSyncContext->mpWorker)
			mpSyncContext->mpWorker->RequestSocketUpdate_Locked(*this);
	}
}

void ATNetStreamSocket::Update_Locked() {
	if (mState == State::Accept) {
		if (!InitSocket_Locked()) {
			QueueError_Locked(ATSocketError::Unknown);
			return;
		}

		UpdateLocalAddress_Locked();
		mState = State::Connected;
		QueueEvent_Locked();
	} else if (mState == State::Connect) {
		mState = State::Connecting;

		if (mConnectAddress.mType == ATSocketAddressType::IPv4)
			mSocketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		else if (mConnectAddress.mType == ATSocketAddressType::IPv6)
			mSocketHandle = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
		else {
			QueueError_Locked(ATSocketError::Unknown);
			return;
		}

		if (mSocketHandle == kATInvalidSocket || !InitSocket_Locked()) {
			QueueError_Locked(ATSocketError::Unknown);
			return;
		}

		if (mConnectAddress.mType == ATSocketAddressType::IPv6) {
			const int v6Only = mbDualStack ? 0 : 1;
			if (setsockopt(mSocketHandle, IPPROTO_IPV6, IPV6_V6ONLY, &v6Only, sizeof v6Only) < 0) {
				QueueError_Locked(ATSocketError::Unknown);
				return;
			}
		}

		ATSocketNativeAddress nativeAddress(mConnectAddress);
		const int result = connect(mSocketHandle, nativeAddress.GetSockAddr(), nativeAddress.GetSockAddrLen());
		if (!result) {
			mState = State::Connected;
			UpdateLocalAddress_Locked();
			QueueEvent_Locked();
		} else if (errno != EINPROGRESS && errno != EALREADY && !ATWouldBlock(errno)) {
			QueueError_Locked(ATSocketError::Unknown);
			return;
		}
	}

	if (mState == State::Connected || mState == State::Closing) {
		if (mbRequestedShutdownRecv && !mbSocketShutdownRecv) {
			mbSocketShutdownRecv = true;
			shutdown(mSocketHandle, SHUT_RD);
		}

		DoWrite_Locked();

		if (mbRequestedShutdownSend && !mbSocketShutdownSend && mWriteBuffer.empty()) {
			mbSocketShutdownSend = true;
			shutdown(mSocketHandle, SHUT_WR);
		}

		if (mState == State::Closing && mWriteBuffer.empty())
			DoClose_Locked();
	} else if (mState == State::Close) {
		if (mSocketHandle != kATInvalidSocket) {
			linger lingerValue {};
			lingerValue.l_onoff = 1;
			lingerValue.l_linger = 0;
			setsockopt(mSocketHandle, SOL_SOCKET, SO_LINGER, &lingerValue, sizeof lingerValue);
		}

		DoClose_Locked();
	}
}

short ATNetStreamSocket::GetPollEvents_Locked() const {
	if (mSocketHandle == kATInvalidSocket)
		return 0;

	if (mState == State::Connecting)
		return POLLIN | POLLOUT;

	if (mState != State::Connected && mState != State::Closing)
		return 0;

	short events = 0;
	if (!mbSocketShutdownRecv && mReadBuffer.size() < kStreamBufferSize)
		events |= POLLIN;
	if (!mWriteBuffer.empty())
		events |= POLLOUT;

	return events;
}

void ATNetStreamSocket::HandlePollEvent_Locked(short events) {
	if (mSocketHandle == kATInvalidSocket)
		return;

	if (mState == State::Connecting && (events & (POLLIN | POLLOUT | POLLERR | POLLHUP))) {
		if (ATGetSocketError(mSocketHandle)) {
			QueueError_Locked(ATSocketError::Unknown);
			return;
		}

		mState = State::Connected;
		UpdateLocalAddress_Locked();
		QueueEvent_Locked();
	}

	if (events & POLLNVAL) {
		QueueError_Locked(ATSocketError::Unknown);
		return;
	}

	if (mState == State::Connected || mState == State::Closing) {
		if (events & (POLLIN | POLLHUP))
			DoRead_Locked();
		if (mState == State::Close)
			return;

		if (events & POLLOUT)
			DoWrite_Locked();
		if (mState == State::Close)
			return;

		if ((events & POLLERR) && ATGetSocketError(mSocketHandle)) {
			QueueError_Locked(ATSocketError::Unknown);
			return;
		}

		if (mState == State::Closing && mWriteBuffer.empty())
			DoClose_Locked();
	}
}

ATSocketStatus ATNetStreamSocket::GetSocketStatus_Locked() const {
	ATSocketStatus status {};

	if (mState != State::Closing) {
		status.mbCanRead = !mReadBuffer.empty();
		status.mbCanWrite = mWriteBuffer.size() < kStreamBufferSize;
	}

	status.mbClosed = mState == State::Closed;
	status.mbConnecting = mState == State::Created || mState == State::Connect || mState == State::Connecting;
	status.mbRemoteClosed = mbSocketRemoteClosed;
	status.mError = mError;
	return status;
}

bool ATNetStreamSocket::InitSocket_Locked() {
	if (!ATSetSocketNonblocking(mSocketHandle))
		return false;

	const int enabled = 1;
	setsockopt(mSocketHandle, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof enabled);
	setsockopt(mSocketHandle, SOL_SOCKET, SO_OOBINLINE, &enabled, sizeof enabled);
	return true;
}

void ATNetStreamSocket::UpdateLocalAddress_Locked() {
	if (mSocketHandle == kATInvalidSocket)
		return;

	sockaddr_storage address {};
	socklen_t addressLen = sizeof address;
	if (!getsockname(mSocketHandle, reinterpret_cast<sockaddr *>(&address), &addressLen))
		mLocalAddress = ATSocketFromNativeAddress(reinterpret_cast<const sockaddr *>(&address));
}

void ATNetStreamSocket::DoRead_Locked() {
	if (mSocketHandle == kATInvalidSocket || mbSocketShutdownRecv)
		return;

	bool received = false;
	while (mReadBuffer.size() < kStreamBufferSize) {
		char buffer[2048];
		const size_t available = kStreamBufferSize - mReadBuffer.size();
		const ssize_t result = recv(mSocketHandle, buffer, std::min(available, sizeof buffer), 0);

		if (result > 0) {
			mReadBuffer.insert(mReadBuffer.end(), buffer, buffer + result);
			received = true;
			continue;
		}

		if (!result) {
			if (!mbSocketRemoteClosed) {
				mbSocketRemoteClosed = true;
				QueueEvent_Locked();
			}
			break;
		}

		if (errno == EINTR)
			continue;
		if (ATWouldBlock(errno))
			break;

		QueueError_Locked(ATSocketError::Unknown);
		return;
	}

	if (received)
		QueueEvent_Locked();
}

void ATNetStreamSocket::DoWrite_Locked() {
	if (mSocketHandle == kATInvalidSocket || mState == State::Connecting)
		return;

	const bool wasFull = mWriteBuffer.size() == kStreamBufferSize;
	while (!mWriteBuffer.empty()) {
		const ssize_t result = send(mSocketHandle, mWriteBuffer.data(), mWriteBuffer.size(), 0);
		if (result > 0) {
			mWriteBuffer.erase(mWriteBuffer.begin(), mWriteBuffer.begin() + result);
			continue;
		}

		if (result < 0 && errno == EINTR)
			continue;
		if (result < 0 && ATWouldBlock(errno))
			break;

		QueueError_Locked(ATSocketError::Unknown);
		return;
	}

	if (wasFull && mWriteBuffer.size() < kStreamBufferSize)
		QueueEvent_Locked();
}

void ATNetStreamSocket::DoClose_Locked() {
	CloseNativeSocket_Locked();
	mState = State::Closed;
	QueueEvent_Locked();

	if (mpSyncContext->mpWorker)
		mpSyncContext->mpWorker->RequestSocketUpdate_Locked(*this);
}

////////////////////////////////////////////////////////////////////////////////

ATNetListenSocket::ATNetListenSocket(ATNetSocketSyncContext& syncContext, const ATSocketAddress& bindAddress, bool dualStack)
	: ATNetSocketT(syncContext)
	, mBindAddress(bindAddress)
	, mbDualStack(dualStack)
{
	mState = State::Listen;
}

ATNetListenSocket::~ATNetListenSocket() {
}

vdrefptr<IATStreamSocket> ATNetListenSocket::Accept() {
	ATSocketNativeHandle pendingSocket = kATInvalidSocket;
	ATSocketAddress pendingAddress;
	ATNetSocketWorker *worker = nullptr;

	vdsynchronized(mpSyncContext->mMutex) {
		pendingSocket = std::exchange(mPendingSocket, kATInvalidSocket);
		pendingAddress = mPendingAddress;
		worker = mpSyncContext->mpWorker;

		if (worker)
			worker->RequestSocketUpdate_Locked(*this);
	}

	if (pendingSocket == kATInvalidSocket)
		return nullptr;

	if (!worker) {
		close(pendingSocket);
		return nullptr;
	}

	return worker->CreateStreamSocket(pendingAddress, pendingSocket);
}

void ATNetListenSocket::Shutdown() {
	vdsynchronized(mpSyncContext->mMutex) {
		if (mPendingSocket != kATInvalidSocket) {
			close(mPendingSocket);
			mPendingSocket = kATInvalidSocket;
		}
	}

	ATNetSocketT::Shutdown();
}

void ATNetListenSocket::Update_Locked() {
	if (mState == State::Listen) {
		mState = State::Listening;

		if (mBindAddress.mType == ATSocketAddressType::IPv4)
			mSocketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		else if (mBindAddress.mType == ATSocketAddressType::IPv6)
			mSocketHandle = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
		else {
			QueueError_Locked(ATSocketError::Unknown);
			return;
		}

		if (mSocketHandle == kATInvalidSocket || !ATSetSocketNonblocking(mSocketHandle)) {
			QueueError_Locked(ATSocketError::Unknown);
			return;
		}

		const int enabled = 1;
		setsockopt(mSocketHandle, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof enabled);

		if (mBindAddress.mType == ATSocketAddressType::IPv6) {
			const int v6Only = mbDualStack ? 0 : 1;
			if (setsockopt(mSocketHandle, IPPROTO_IPV6, IPV6_V6ONLY, &v6Only, sizeof v6Only) < 0) {
				QueueError_Locked(ATSocketError::Unknown);
				return;
			}
		}

		ATSocketNativeAddress nativeAddress(mBindAddress);
		if (bind(mSocketHandle, nativeAddress.GetSockAddr(), nativeAddress.GetSockAddrLen()) < 0
			|| listen(mSocketHandle, SOMAXCONN) < 0) {
			QueueError_Locked(ATSocketError::Unknown);
			return;
		}

		TryAccept_Locked();
	} else if (mState == State::Close || mState == State::Closing) {
		CloseNativeSocket_Locked();
		mState = State::Closed;
		QueueEvent_Locked();

		if (mpSyncContext->mpWorker)
			mpSyncContext->mpWorker->RequestSocketUpdate_Locked(*this);
	}
}

short ATNetListenSocket::GetPollEvents_Locked() const {
	return mSocketHandle != kATInvalidSocket && mState == State::Listening && mPendingSocket == kATInvalidSocket
		? POLLIN
		: 0;
}

void ATNetListenSocket::HandlePollEvent_Locked(short events) {
	if (events & POLLNVAL) {
		QueueError_Locked(ATSocketError::Unknown);
		return;
	}

	if (events & POLLIN)
		TryAccept_Locked();

	if ((events & POLLERR) && ATGetSocketError(mSocketHandle))
		QueueError_Locked(ATSocketError::Unknown);
	else if (events & POLLHUP)
		QueueError_Locked(ATSocketError::Unknown);
}

ATSocketStatus ATNetListenSocket::GetSocketStatus_Locked() const {
	ATSocketStatus status {};
	status.mbClosed = mState == State::Closed;
	status.mbCanAccept = mPendingSocket != kATInvalidSocket;
	status.mError = mError;
	return status;
}

void ATNetListenSocket::TryAccept_Locked() {
	if (mPendingSocket != kATInvalidSocket || mSocketHandle == kATInvalidSocket)
		return;

	sockaddr_storage address {};
	socklen_t addressLen = sizeof address;
	const int socketHandle = accept(mSocketHandle, reinterpret_cast<sockaddr *>(&address), &addressLen);
	if (socketHandle < 0) {
		if (errno != EINTR && !ATWouldBlock(errno))
			QueueError_Locked(ATSocketError::Unknown);
		return;
	}

	mPendingAddress = ATSocketFromNativeAddress(reinterpret_cast<const sockaddr *>(&address));
	mPendingSocket = socketHandle;
	QueueEvent_Locked();
}

////////////////////////////////////////////////////////////////////////////////

ATNetDatagramSocket::ATNetDatagramSocket(ATNetSocketSyncContext& syncContext, const ATSocketAddress& bindAddress, bool dualStack)
	: ATNetSocketT(syncContext)
	, mBindAddress(bindAddress)
	, mbDualStack(dualStack)
{
	mState = State::Connect;
}

ATNetDatagramSocket::~ATNetDatagramSocket() {
}

sint32 ATNetDatagramSocket::RecvFrom(ATSocketAddress& address, void *data, uint32 maxlen) {
	vdsynchronized(mpSyncContext->mMutex) {
		if (mState != State::Connect && mState != State::Connected)
			return -1;

		while (!mReadQueue.empty()) {
			Packet packet = std::move(mReadQueue.front());
			mReadQueue.pop_front();
			mReadBytes -= packet.mData.size() + sizeof(ATSocketAddress) + sizeof(uint16);

			if (mpSyncContext->mpWorker)
				mpSyncContext->mpWorker->RequestSocketUpdate_Locked(*this);

			if (packet.mData.size() > maxlen)
				continue;

			address = packet.mAddress;
			memcpy(data, packet.mData.data(), packet.mData.size());
			return (sint32)packet.mData.size();
		}

		return -1;
	}
}

bool ATNetDatagramSocket::SendTo(const ATSocketAddress& address, const void *data, uint32 len) {
	if (len > kMaxDatagramSize)
		return false;

	vdsynchronized(mpSyncContext->mMutex) {
		if (mState != State::Connect && mState != State::Connected)
			return false;

		const size_t packetBytes = len + sizeof(ATSocketAddress) + sizeof(uint16);
		if (mWriteBytes + packetBytes > kBufferSize)
			return false;

		Packet packet;
		packet.mAddress = address;
		if (mBindAddress.mType == ATSocketAddressType::IPv6 && packet.mAddress.mType == ATSocketAddressType::IPv4)
			packet.mAddress = ATSocketAddress::CreateIPv4InIPv6(packet.mAddress);

		if (len) {
			const uint8 *src = static_cast<const uint8 *>(data);
			packet.mData.assign(src, src + len);
		}
		mWriteQueue.emplace_back(std::move(packet));
		mWriteBytes += packetBytes;

		if (mpSyncContext->mpWorker)
			mpSyncContext->mpWorker->RequestSocketUpdate_Locked(*this);

		return true;
	}
}

void ATNetDatagramSocket::Update_Locked() {
	if (mState == State::Connect) {
		if (mBindAddress.mType == ATSocketAddressType::IPv4)
			mSocketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		else if (mBindAddress.mType == ATSocketAddressType::IPv6)
			mSocketHandle = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
		else {
			QueueError_Locked(ATSocketError::Unknown);
			return;
		}

		if (mSocketHandle == kATInvalidSocket || !ATSetSocketNonblocking(mSocketHandle)) {
			QueueError_Locked(ATSocketError::Unknown);
			return;
		}

		if (mBindAddress.mType == ATSocketAddressType::IPv6) {
			const int v6Only = mbDualStack ? 0 : 1;
			if (setsockopt(mSocketHandle, IPPROTO_IPV6, IPV6_V6ONLY, &v6Only, sizeof v6Only) < 0) {
				QueueError_Locked(ATSocketError::Unknown);
				return;
			}
		}

		if (mBindAddress.IsNonZero()) {
			ATSocketNativeAddress nativeAddress(mBindAddress);
			if (bind(mSocketHandle, nativeAddress.GetSockAddr(), nativeAddress.GetSockAddrLen()) < 0) {
				QueueError_Locked(ATSocketError::Unknown);
				return;
			}
		}

		mState = State::Connected;
		QueueEvent_Locked();
	}

	if (mState == State::Connected || mState == State::Closing) {
		DoRead_Locked();
		DoWrite_Locked();

		if (mState == State::Closing && mWriteQueue.empty())
			DoClose_Locked();
	} else if (mState == State::Close) {
		DoClose_Locked();
	}
}

short ATNetDatagramSocket::GetPollEvents_Locked() const {
	if (mSocketHandle == kATInvalidSocket || (mState != State::Connected && mState != State::Closing))
		return 0;

	short events = 0;
	if (mState == State::Connected && mReadBytes + kMaxDatagramSize + sizeof(ATSocketAddress) + sizeof(uint16) <= kBufferSize)
		events |= POLLIN;
	if (!mWriteQueue.empty())
		events |= POLLOUT;
	return events;
}

void ATNetDatagramSocket::HandlePollEvent_Locked(short events) {
	if (events & POLLNVAL) {
		QueueError_Locked(ATSocketError::Unknown);
		return;
	}

	if (events & POLLIN)
		DoRead_Locked();
	if (mState == State::Close)
		return;
	if (events & POLLOUT)
		DoWrite_Locked();
	if (mState == State::Close)
		return;

	if ((events & POLLERR) && ATGetSocketError(mSocketHandle))
		QueueError_Locked(ATSocketError::Unknown);

	if (mState == State::Closing && mWriteQueue.empty())
		DoClose_Locked();
}

ATSocketStatus ATNetDatagramSocket::GetSocketStatus_Locked() const {
	ATSocketStatus status {};
	status.mbConnecting = mState == State::Connect;
	status.mbCanRead = !mReadQueue.empty();
	status.mbCanWrite = mWriteBytes + kMaxDatagramSize + sizeof(ATSocketAddress) + sizeof(uint16) <= kBufferSize;
	status.mbClosed = mState == State::Closed;
	status.mError = mError;
	return status;
}

void ATNetDatagramSocket::DoRead_Locked() {
	if (mSocketHandle == kATInvalidSocket || mState != State::Connected)
		return;

	bool received = false;
	while (mReadBytes + kMaxDatagramSize + sizeof(ATSocketAddress) + sizeof(uint16) <= kBufferSize) {
		Packet packet;
		packet.mData.resize(kMaxDatagramSize);

		sockaddr_storage address {};
		socklen_t addressLen = sizeof address;
		const ssize_t result = recvfrom(
			mSocketHandle,
			packet.mData.data(),
			packet.mData.size(),
			0,
			reinterpret_cast<sockaddr *>(&address),
			&addressLen);

		if (result < 0) {
			if (errno == EINTR)
				continue;
			if (ATWouldBlock(errno))
				break;

			QueueError_Locked(ATSocketError::Unknown);
			return;
		}

		packet.mAddress = ATSocketFromNativeAddress(reinterpret_cast<const sockaddr *>(&address));
		packet.mData.resize((size_t)result);
		mReadBytes += packet.mData.size() + sizeof(ATSocketAddress) + sizeof(uint16);
		mReadQueue.emplace_back(std::move(packet));
		received = true;
	}

	if (received)
		QueueEvent_Locked();
}

void ATNetDatagramSocket::DoWrite_Locked() {
	if (mSocketHandle == kATInvalidSocket)
		return;

	const bool wasFull = mWriteBytes + kMaxDatagramSize + sizeof(ATSocketAddress) + sizeof(uint16) > kBufferSize;
	while (!mWriteQueue.empty()) {
		const Packet& packet = mWriteQueue.front();
		ATSocketNativeAddress nativeAddress(packet.mAddress);
		const ssize_t result = sendto(
			mSocketHandle,
			packet.mData.data(),
			packet.mData.size(),
			0,
			nativeAddress.GetSockAddr(),
			nativeAddress.GetSockAddrLen());

		if (result < 0) {
			if (errno == EINTR)
				continue;
			if (ATWouldBlock(errno))
				break;

			QueueError_Locked(ATSocketError::Unknown);
			return;
		}

		mWriteBytes -= packet.mData.size() + sizeof(ATSocketAddress) + sizeof(uint16);
		mWriteQueue.pop_front();
	}

	if (wasFull && mWriteBytes + kMaxDatagramSize + sizeof(ATSocketAddress) + sizeof(uint16) <= kBufferSize)
		QueueEvent_Locked();
}

void ATNetDatagramSocket::DoClose_Locked() {
	CloseNativeSocket_Locked();
	mState = State::Closed;
	QueueEvent_Locked();

	if (mpSyncContext->mpWorker)
		mpSyncContext->mpWorker->RequestSocketUpdate_Locked(*this);
}

////////////////////////////////////////////////////////////////////////////////

ATNetSocketWorker::ATNetSocketWorker()
	: VDThread("Net socket worker")
{
	mpSyncContext = new ATNetSocketSyncContext;
	mpSyncContext->mpWorker = this;
}

ATNetSocketWorker::~ATNetSocketWorker() {
	Shutdown();
}

bool ATNetSocketWorker::Init() {
	int wakeHandles[2];
	if (pipe(wakeHandles) < 0)
		return false;

	mWakeReadHandle = wakeHandles[0];
	mWakeWriteHandle = wakeHandles[1];
	ATSetSocketNonblocking(mWakeReadHandle);
	ATSetSocketNonblocking(mWakeWriteHandle);

	if (!ThreadStart()) {
		close(mWakeReadHandle);
		close(mWakeWriteHandle);
		mWakeReadHandle = -1;
		mWakeWriteHandle = -1;
		return false;
	}

	return true;
}

void ATNetSocketWorker::Shutdown() {
	if (mWakeReadHandle < 0)
		return;

	vdsynchronized(mpSyncContext->mMutex) {
		mbExitRequested = true;
	}
	Wake();
	ThreadWait();

	close(mWakeReadHandle);
	close(mWakeWriteHandle);
	mWakeReadHandle = -1;
	mWakeWriteHandle = -1;
}

vdrefptr<ATNetStreamSocket> ATNetSocketWorker::CreateStreamSocket() {
	vdrefptr<ATNetStreamSocket> socket(new ATNetStreamSocket(*mpSyncContext));
	bool registered;

	vdsynchronized(mpSyncContext->mMutex) {
		registered = RegisterSocket_Locked(*socket);
	}
	if (!registered)
		return nullptr;

	return socket;
}

vdrefptr<ATNetStreamSocket> ATNetSocketWorker::CreateStreamSocket(const ATSocketAddress& connectedAddress, ATSocketNativeHandle nativeSocket) {
	vdrefptr<ATNetStreamSocket> socket(new ATNetStreamSocket(*mpSyncContext, connectedAddress, nativeSocket));
	bool registered;

	vdsynchronized(mpSyncContext->mMutex) {
		registered = RegisterSocket_Locked(*socket);
	}
	if (!registered) {
		socket->Shutdown();
		return nullptr;
	}

	return socket;
}

vdrefptr<ATNetListenSocket> ATNetSocketWorker::CreateListenSocket(const ATSocketAddress& bindAddress, bool dualStack) {
	vdrefptr<ATNetListenSocket> socket(new ATNetListenSocket(*mpSyncContext, bindAddress, dualStack));
	bool registered;

	vdsynchronized(mpSyncContext->mMutex) {
		registered = RegisterSocket_Locked(*socket);
	}
	if (!registered)
		return nullptr;

	return socket;
}

vdrefptr<ATNetDatagramSocket> ATNetSocketWorker::CreateDatagramSocket(const ATSocketAddress& bindAddress, bool dualStack) {
	vdrefptr<ATNetDatagramSocket> socket(new ATNetDatagramSocket(*mpSyncContext, bindAddress, dualStack));
	bool registered;

	vdsynchronized(mpSyncContext->mMutex) {
		registered = RegisterSocket_Locked(*socket);
	}
	if (!registered)
		return nullptr;

	return socket;
}

void ATNetSocketWorker::RequestSocketUpdate_Locked(const ATNetSocket& socket) {
	if (socket.mSocketIndex < 0)
		return;

	mSocketsNeedUpdate[(size_t)socket.mSocketIndex] = true;
	if (!mbUpdateSockets) {
		mbUpdateSockets = true;
		Wake();
	}
}

bool ATNetSocketWorker::RegisterSocket_Locked(ATNetSocket& socket) {
	if (mNumSockets >= kMaxSockets || mbExitRequested)
		return false;

	mSocketTable[mNumSockets] = &socket;
	socket.mSocketIndex = mNumSockets++;
	RequestSocketUpdate_Locked(socket);
	return true;
}

void ATNetSocketWorker::Wake() {
	if (mWakeWriteHandle < 0)
		return;

	const uint8 value = 1;
	while (write(mWakeWriteHandle, &value, sizeof value) < 0 && errno == EINTR) {
	}
}

void ATNetSocketWorker::ThreadRun() {
	for (;;) {
		std::vector<vdrefptr<ATNetSocket>> socketsToFlush;
		std::vector<vdrefptr<ATNetSocket>> socketsToDestroy;

		pollfd pollDescriptors[kMaxSockets + 1] {};
		ATNetSocket *pollSockets[kMaxSockets] {};
		size_t pollSocketCount = 0;

		vdsynchronized(mpSyncContext->mMutex) {
			if (mbExitRequested)
				break;

			if (mbUpdateSockets) {
				mbUpdateSockets = false;

				for (size_t i = 0; i < mNumSockets;) {
					if (!mSocketsNeedUpdate[i]) {
						++i;
						continue;
					}

					mSocketsNeedUpdate[i] = false;
					ATNetSocket *socket = mSocketTable[i];
					if (socket->IsAbandoned()) {
						socket->mSocketIndex = -1;
						socketsToDestroy.emplace_back(std::move(mSocketTable[i]));
						--mNumSockets;

						if (i < mNumSockets) {
							mSocketTable[i] = std::move(mSocketTable[mNumSockets]);
							mSocketTable[i]->mSocketIndex = (int)i;
							mSocketsNeedUpdate[i] = mSocketsNeedUpdate[mNumSockets];
						}

						mSocketTable[mNumSockets] = nullptr;
						mSocketsNeedUpdate[mNumSockets] = false;
						continue;
					}

					socket->Update_Locked();
					socketsToFlush.emplace_back(socket);
					++i;
				}
			}

			pollDescriptors[0].fd = mWakeReadHandle;
			pollDescriptors[0].events = POLLIN;

			for (size_t i = 0; i < mNumSockets; ++i) {
				ATNetSocket *socket = mSocketTable[i];
				pollDescriptors[pollSocketCount + 1].fd = socket->mSocketHandle;
				pollDescriptors[pollSocketCount + 1].events = socket->GetPollEvents_Locked();
				pollSockets[pollSocketCount++] = socket;
			}
		}

		for (auto& socket : socketsToFlush)
			socket->FlushEvent();
		for (auto& socket : socketsToDestroy)
			socket->Shutdown();
		socketsToFlush.clear();
		socketsToDestroy.clear();

		int pollResult;
		do {
			pollResult = poll(pollDescriptors, (nfds_t)pollSocketCount + 1, -1);
		} while (pollResult < 0 && errno == EINTR);

		if (pollResult < 0)
			break;

		if (pollDescriptors[0].revents) {
			uint8 buffer[64];
			while (read(mWakeReadHandle, buffer, sizeof buffer) > 0) {
			}
			continue;
		}

		socketsToFlush.clear();
		vdsynchronized(mpSyncContext->mMutex) {
			for (size_t i = 0; i < pollSocketCount; ++i) {
				if (!pollDescriptors[i + 1].revents)
					continue;

				ATNetSocket *socket = pollSockets[i];
				socket->HandlePollEvent_Locked(pollDescriptors[i + 1].revents);
				socketsToFlush.emplace_back(socket);
			}
		}

		for (auto& socket : socketsToFlush)
			socket->FlushEvent();
	}

	std::vector<vdrefptr<ATNetSocket>> socketsToDestroy;
	vdsynchronized(mpSyncContext->mMutex) {
		mpSyncContext->mpWorker = nullptr;

		for (auto& socket : mSocketTable) {
			if (socket) {
				socket->mSocketIndex = -1;
				socketsToDestroy.emplace_back(std::move(socket));
			}
		}

		mNumSockets = 0;
		mSocketsNeedUpdate.reset();
	}

	for (auto& socket : socketsToDestroy)
		socket->Shutdown();
}
