#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/thread/future.hpp>
#include <any>
#include <algorithm>
#include <iostream>
#include <thread>
#include <hiredis/hiredis.h>

#ifdef _MSC_VER
#include <winsock2.h> /* For struct timeval */
#endif


namespace asio = boost::asio;

template <typename Token, typename ReturnType = std::pair<bool, std::any>>
typename asio::async_result<typename std::decay<Token>::type,
	void(boost::system::error_code, ReturnType)>::return_type
	AsyncRedisCommand(Token&& token, std::string cmd) {
	auto init = [cmd = std::move(cmd)](auto handler) mutable {
		std::thread(
			[cmd = std::move(cmd), handler = std::move(handler)]() mutable {
				boost::system::error_code ec;
				std::any result;
				redisContext* c;
				redisReply* reply;
				const char* hostname = "127.0.0.1";
				int port = 6379;
				struct timeval timeout = { 1, 500000 }; // 1.5 seconds
				c = redisConnectWithTimeout(hostname, port, timeout);
				if (c == NULL || c->err) {
					if (c) {
						result = std::string(c->errstr);
						redisFree(c);
					}
					else {
						result = std::string("can't allocate redis context!");
					}

					auto executor = asio::get_associated_executor(handler);
					asio::dispatch(executor, [ec, handler = std::move(handler), result = std::move(result)]() mutable {
						std::move(handler)(ec, std::make_pair(false, result));
					});
					return;
				}

				bool ok = true;
				reply = (redisReply*)redisCommand(c, cmd.c_str());
				if (reply == nullptr) {
					result = std::string(c->errstr);
					auto executor = asio::get_associated_executor(handler);
					asio::dispatch(executor, [ec, handler = std::move(handler), result = std::move(result)]() mutable {
						std::move(handler)(ec, std::make_pair(false, result));
					});
					freeReplyObject(reply);
					redisFree(c);
				}
				switch (reply->type) {
				case REDIS_REPLY_STRING:
					result = std::string(reply->str, reply->len);
					break;
				case REDIS_REPLY_INTEGER:
					result = reply->integer;
					break;
				case REDIS_REPLY_NIL:
					break;
				case REDIS_REPLY_ERROR:
					result = std::string(reply->str, reply->len);
					ok = false;
					break;
				case REDIS_REPLY_DOUBLE:
					result = reply->dval;
					break;
				default:
					result = std::string("unhandled return type!") + std::to_string(reply->type);
					ok = false;
				}
				freeReplyObject(reply);
				redisFree(c);
				auto executor = asio::get_associated_executor(handler);
				asio::dispatch(executor, [ec, handler = std::move(handler), ok, result = std::move(result)]() mutable {
					std::move(handler)(ec, std::make_pair(ok, result));
				});
		}).detach();
	};

	return asio::async_initiate<Token, void(boost::system::error_code, ReturnType)>(init, token);
}

boost::asio::awaitable<void> MyCoroutine() {
	std::cout << "Enter MyCoroutine" << std::endl;

	co_await AsyncRedisCommand(boost::asio::use_awaitable, "SET msg hello,world!");

	auto [ok, res] = co_await AsyncRedisCommand(boost::asio::use_awaitable, "GET msg");
	if (!ok)
		std::cerr << "error! " << std::any_cast<std::string>(res) << std::endl;
	else
		std::cout << "name=" << std::any_cast<std::string>(res) << std::endl;

	std::cout << "Leave MyCoroutine" << std::endl;
}

int main() {
	asio::io_context ioc;
	asio::co_spawn(ioc, MyCoroutine, boost::asio::detached);
	ioc.run();
}
