//
// Created by S on 2016/06/22.
//

#ifndef HARDGRAD_MOBILE_ERRORS_H
#define HARDGRAD_MOBILE_ERRORS_H

// An error or Ok
struct Error
{
	bool hasValue;
	std::string message;
	Error() : hasValue(false), message("") {}
	Error(const std::string& msg) : hasValue(true), message(msg) {}
public:
	static auto succ() { return Error(); }
	static auto fail(const std::string& msg) { return Error(msg); }
	static auto failIf(bool cond, const std::string& msg) { return cond ? Error::fail(msg) : Error::succ(); }

	// Assertion(false) on Error
	auto assertFailure()
	{
		if(this->hasValue)
		{
			__android_log_print(ANDROID_LOG_FATAL, "HardGrad", this->message.c_str());
			assert(false);
		}
	}
};
// Error or Ok with value
template<typename ValueT> class Result
{
	bool has_value;
	ValueT value;
	std::string msg;
	Result(const ValueT& val) : value(val), has_value(true) {}
	Result(const std::string& msg) : msg(msg), has_value(false) {}
public:
	static auto err(const std::string& msg) { return Result<ValueT>(msg); }
	static auto ok(const ValueT& value) { return Result<ValueT>(value); }

	auto unwrap() const
	{
		if(this->has_value) return this->value;
		else
		{
			Error::fail(this->msg).assertFailure();
			return ValueT();
		}
	}
};

#endif //HARDGRAD_MOBILE_ERRORS_H
