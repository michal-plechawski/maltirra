//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2023 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along
//	with this program. If not, see <http://www.gnu.org/licenses/>.

#include <stdafx.h>
#include <vd2/system/thread.h>
#include <vd2/system/time.h>
#include <vd2/system/vdalloc.h>
#include <at/atcore/asyncdispatcherimpl.h>
#include <at/atnetworksockets/nativesockets.h>
#include <test.h>
#include <WinSock2.h>
#include <algorithm>
#include <memory>

namespace {
	struct ATNativeSocketDeleter {
		void operator()(SOCKET s) const {
			AT_TEST_ASSERT(s != INVALID_SOCKET);
			closesocket(s);
		}
	};

	class ATNativeSocketHandle {
	public:
		ATNativeSocketHandle() = default;
		explicit ATNativeSocketHandle(SOCKET s)
			: mSocket(s)
		{
		}

		ATNativeSocketHandle(ATNativeSocketHandle&& src) noexcept
			: mSocket(src.mSocket)
		{
			src.mSocket = INVALID_SOCKET;
		}

		~ATNativeSocketHandle() {
			reset();
		}

		ATNativeSocketHandle& operator=(ATNativeSocketHandle&& src) noexcept {
			std::swap(mSocket, src.mSocket);
			return *this;
		}

		void reset(SOCKET s = INVALID_SOCKET) {
			if (mSocket != INVALID_SOCKET)
				closesocket(mSocket);

			mSocket = s;
		}

		bool valid() const { return mSocket != INVALID_SOCKET; }
		SOCKET get() const { return mSocket; }

		auto operator<=>(const ATNativeSocketHandle& other) const = default;

	private:
		SOCKET mSocket = INVALID_SOCKET;
	};
}

AT_DEFINE_TEST(Net_NativeSockets) {
	VDSignal asyncDispatchSignal;
	ATAsyncDispatcher asyncDispatcher;

	asyncDispatcher.SetWakeCallback(
		[&] { asyncDispatchSignal.signal(); }
	);

	ATSocketStatus status;

	const auto initSocket = [&](IATSocket& socket) {
		status = socket.GetSocketStatus();

		socket.SetOnEvent(&asyncDispatcher, [&](const ATSocketStatus& status2) { status = status2; }, true);
	};

	const auto waitForSocketStatus = [&](const char *stage, const vdfunction<bool(const ATSocketStatus&)>& fn) {
		const uint32 deadline = VDGetCurrentTick() + 3000;
		for(;;) {
			asyncDispatcher.RunCallbacks();
			AT_TEST_ASSERTF(status.mError == ATSocketError::None, "%s: socket error %u", stage, (unsigned)status.mError);
			if (fn(status))
				break;

			const sint32 remaining = (sint32)(deadline - VDGetCurrentTick());
			AT_TEST_ASSERTF(remaining > 0, "%s timed out (connecting=%d, closed=%d, remoteClosed=%d)",
				stage, status.mbConnecting, status.mbClosed, status.mbRemoteClosed);
			asyncDispatchSignal.tryWait(std::min<uint32>((uint32)remaining, 20));
		}
	};

	AT_TEST_ASSERT(ATSocketInit());

	{
		ATNativeSocketHandle hs(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
		AT_TEST_ASSERT(hs.valid());

		sockaddr_in localhost {};
		localhost.sin_family = AF_INET;
		localhost.sin_port = 0;
		localhost.sin_addr.S_un.S_addr = htonl(INADDR_LOOPBACK);
		AT_TEST_ASSERT(0 == bind(hs.get(), (const sockaddr *)&localhost, sizeof localhost));
		int localhostLen = sizeof localhost;
		AT_TEST_ASSERT(0 == getsockname(hs.get(), (sockaddr *)&localhost, &localhostLen));
		AT_TEST_ASSERT(localhost.sin_port != 0);
		AT_TEST_ASSERT(0 == listen(hs.get(), SOMAXCONN));

		auto s = ATNetConnect(ATSocketAddress::CreateIPv4(0x7F000001, ntohs(localhost.sin_port)));
		AT_TEST_ASSERT(s);

		initSocket(*s);

		AT_TEST_ASSERT(!status.mbRemoteClosed);
		AT_TEST_ASSERT(!status.mbClosed);
		AT_TEST_ASSERT(!status.mbCanRead);
		AT_TEST_ASSERT(status.mbCanWrite);
		AT_TEST_ASSERT(!status.mbCanAccept);
		AT_TEST_ASSERT(status.mError == ATSocketError::None);

		// Connecting is transient and can be over before the first snapshot.
		// Wait for the connected state before accepting the native peer.
		waitForSocketStatus("connect", [](const ATSocketStatus& status) { return !status.mbConnecting; });

		sockaddr_in srcaddr {};
		int srcaddrlen = sizeof(srcaddr);
		ATNativeSocketHandle hs2(accept(hs.get(), (sockaddr *)&srcaddr, &srcaddrlen));
		AT_TEST_ASSERT(hs2.valid());

		AT_TEST_ASSERT(!status.mbRemoteClosed);
		AT_TEST_ASSERT(!status.mbClosed);
		AT_TEST_ASSERT(!status.mbCanRead);
		AT_TEST_ASSERT(status.mbCanWrite);
		AT_TEST_ASSERT(!status.mbCanAccept);
		AT_TEST_ASSERT(status.mError == ATSocketError::None);

		AT_TEST_ASSERT(0 == shutdown(hs2.get(), SD_SEND));

		waitForSocketStatus("remote shutdown", [](const ATSocketStatus& status) { return status.mbRemoteClosed; });
		AT_TEST_ASSERT(!status.mbClosed);
		AT_TEST_ASSERT(!status.mbCanRead);
		AT_TEST_ASSERT(status.mbCanWrite);
		AT_TEST_ASSERT(!status.mbCanAccept);
		AT_TEST_ASSERT(status.mError == ATSocketError::None);

		s->CloseSocket(false);
		waitForSocketStatus("graceful close", [](const ATSocketStatus& status) { return status.mbClosed; });

		char c;
		AT_TEST_ASSERT(recv(hs2.get(), &c, 1, 0) == 0);
	}

	ATSocketShutdown();
	return 0;
}
