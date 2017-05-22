#pragma once

#include "fact/CircularBuffer.h"
#include <cstdlib>
#include "fact/string_convert.h"
#include "ios.h"
#include "experimental.h"


#include "fact/string.h"

#define DEBUG
// NOTE: Need DEBUG_IOS_GETLINE on because we haven't put unget into our getline call fully yet
#define DEBUG_IOS_GETLINE
//#define DEBUG_ATC_INPUT
//#define DEBUG_ATC_MATCH
//#define DEBUG_ATC_UNGET
//#define DEBUG_ATC_OUTPUT
#if defined(DEBUG_ATC_INPUT) and defined(DEBUG_ATC_OUTPUT)
#define DEBUG_ATC_ECHO
#endif
// FIX: really should splice in a different istream
//#define DEBUG_SIMULATED
#define DEBUG_ATC_CONTEXT

// FIX: This seems to work based on preliminary tests, but I haven't gotten debug_context
// working with it yet
//#define FEATURE_DISCRETE_PARSER

// Do not use istream peek, make our own
//#define FEATURE_ATC_PEEK

#include "DebugContext.h"


namespace layer3 {

// TODO: revisit name of this class
// NOTE: keywords must itself be NULL terminated.  This can be optimized by turning this into a layer-2
// TODO: Add null-terminated arrays to fact.util.embedded buffer.h
class MultiMatcher
{
    const char** keywords;
    uint8_t hpos = 0, vpos = 0;
    bool match = false;

public:
    MultiMatcher(const char** keywords) : keywords(keywords) {}

    bool parse(char c);

    void reset()
    {
        match = false;
        hpos = 0;
        vpos = 0;
    }

    bool is_matched() const { return match; };
    const char* matched() const { return keywords[vpos]; }

    static const char* do_match(const char* input, const char** keywords);
};

}

class ATBuilder;

class ATCommander
#ifdef FEATURE_DISCRETE_PARSER
    : public experimental::ParserWrapper
#endif
{
    static constexpr char WHITESPACE_NEWLINE[] = " \r\n";

#ifndef FEATURE_DISCRETE_PARSER
    const char* delimiters = WHITESPACE_NEWLINE;
#endif

#ifdef FEATURE_ATC_PEEK
    char cache = 0;
#endif

    const char* error_category = nullptr;
    const char* error_description = nullptr;

    template <typename T>
    friend ATCommander& operator >>(ATCommander& atc, T);

    friend class ATBuilder;

    // TODO: callback/event mechanism to fire when errors happen

protected:
    void set_error(const char* error, const char* description)
    {
#ifdef DEBUG
        fstd::clog << "ATCommander error: " << error << ": " << description << fstd::endl;
#endif
        this->error_category = error;
        this->error_description = description;
    }


    void _send() {}

    template <class T, class ...TArgs>
    void _send(T value, TArgs...args)
    {
        *this << value;
        _send(args...);
    }


    void _match() {}

    template <class T, class ...TArgs>
    void _match(T value, TArgs...args)
    {
        _input_match(value);
        _match(args...);
    }


public:
    static constexpr char OK[] = "OK";
    static constexpr char AT[] = "AT";
    static constexpr char ERROR[] = "ERROR";

    DebugContext<AT> debug_context;

    class _error_struct
    {
        typedef uint8_t status;

        status _status;

        static constexpr status at_result_bit = 0x01;   // When ERROR comes back from an AT command
        static constexpr status transport_bit = 0x02;   // When underlying cin/cout has an issue

    public:
        void reset() { _status = 0; }

        bool at_result() { return _status & at_result_bit; }
        void set_at_result() { _status |= at_result_bit; }

    } error;


    struct _experimental
    {
        typedef uint8_t input_processing_flags;

        static constexpr input_processing_flags eat_delimiter_bit = 0x01;
        // FIX: this is an OUTPUT processing flag
        static constexpr input_processing_flags auto_delimit_bit = 0x02;

        //input_processing_flags _input_processing_flags;

        // TODO: do this later after splitting into OutputFormatter and InputFormatter
        //template <char output_delimiter = 0>
        class Formatter
        {
            ATCommander& atc;
            input_processing_flags flags;

            template <typename T>
            friend Formatter& operator >>(Formatter& atc, T);

            template <typename T>
            friend Formatter& operator <<(Formatter& atc, T);

        public:
            Formatter(ATCommander& atc) : atc(atc) {}
            ~Formatter() { atc.reset_delimiters(); }

            void set_auto_delimit()
            {
                flags |= auto_delimit_bit;
            }

            bool auto_delimit()
            {
                return flags & auto_delimit_bit;
            }

            void set_eat_delimiter()
            { flags |= eat_delimiter_bit; }

            bool eat_delimiter()
            { return flags & eat_delimiter_bit; }

            void eat_delimiters(const char* delimiters)
            {
                atc.set_delimiter(delimiters);
                set_eat_delimiter();
            }

        };
    } experimental;


#ifdef DEBUG_SIMULATED
    FactUtilEmbedded::layer1::CircularBuffer<char, 128> debugBuffer;
#endif


    bool is_in_error() { return error_description; }
    void reset_error() { error_description = nullptr; }
    const char* get_error() { return error_description; }

#ifdef FEATURE_ATC_PEEK
    // TODO: fix this name
    bool is_cached() { return cache != 0; }
#endif

#ifndef FEATURE_DISCRETE_PARSER
    fstd::istream& cin;
#endif
    fstd::ostream& cout;

    ATCommander(fstd::istream& cin, fstd::ostream& cout) :
#ifdef FEATURE_DISCRETE_PARSER
        ParserWrapper(cin),
#else
        cin(cin),
#endif
        cout(cout)
    {
#ifdef DEBUG_ATC_ECHO
        fstd::clog << "ATCommander: Full echo mode" << fstd::endl;
#endif

#ifdef FEATURE_DISCRETE_PARSER
        set_delimiter(WHITESPACE_NEWLINE);
#endif
    }


#ifndef FEATURE_DISCRETE_PARSER
    void set_delimiter(const char* delimiters)
    {
        this->delimiters = delimiters;
    }


    void reset_delimiters()
    {
        this->delimiters = WHITESPACE_NEWLINE;
    }
#else
    void reset_delimiters()
    {
        parser.set_delimiter(WHITESPACE_NEWLINE);
    }
#endif

    int _get()
    {
#ifdef FEATURE_ATC_PEEK
        if(is_cached())
        {
#ifdef DEBUG_ATC_UNGET
            fstd::clog << "Yanking from a previous unget: " << (int)cache << fstd::endl;
#endif
            char temp = cache;
            cache = 0;
            return temp;
        }
#endif

#ifdef DEBUG_SIMULATED
        return debugBuffer.available() ? debugBuffer.get() : -1;
#else
        return cin.get();
#endif
    }

    int get()
    {
        int ch = _get();
#ifdef DEBUG_ATC_ECHO
        // TODO: buffer this output and spit out only at newlines, very reminiscent of how actual clog
        // should work
        fstd::clog.put(ch);
#endif
        return ch;
    }




    void getline(char* s, fstd::streamsize max, const char terminator = '\n')
    {
#if defined(DEBUG_SIMULATED) or defined(DEBUG_IOS_GETLINE)
        int ch;
        int len = 0;

        while((ch = get()) != terminator && ch != -1)
        {
            // FIX: hacky also and will cause bugs for real getline usage
            if(ch != 13) s[len++] = ch;
        }

        s[len] = 0;
#else
        cin.getline(s, max, terminator);

        //return result;
#endif
#ifdef DEBUG_ATC_INPUT
        fstd::clog << "Getline: '" << s << "' (terminated from character# " << ch << ")\r\n";
#endif
    }


#if defined(FEATURE_IOS_UNGET) or defined(FEATURE_ATC_PEEK)
    void unget(char ch)
    {
#ifdef FEATURE_ATC_PEEK
#ifdef DEBUG_SIMULATED_BROKEN
        // TODO: need circular buffer unget
        //debugBuffer.put(ch);
#else
        cache = ch;
#endif
#else
        cin.unget();
#endif
    }
#endif

    int peek()
    {
#if defined(FEATURE_ATC_PEEK)
        // FIX: quite broken.  really need that debugBuffer...
        return cache;
#else
        return cin.peek();
#endif
    }


    // Blocking peek operation with a timeout
    int peek_timeout_experimental(uint32_t timeout_ms)
    {
#ifdef FEATURE_ATC_PEEK
        // no timeouts used old-style ATC peek code, they block more frequently
        // in other areas
        return peek();
#endif
        uint32_t timeout = experimental::millis() + timeout_ms;
        int ch;

        while(((ch = peek()) == EOF) && (experimental::millis() < timeout))
            experimental::yield();

#ifdef DEBUG_ATC_INPUT
        if(ch == -1)
            fstd::clog << "Timeout while peeking: " << timeout << fstd::endl;
#endif

        return ch;
    }

    // Blocking get operation with a timeout
    int get_timeout_experimental(uint16_t timeout_ms)
    {
        int ch = peek_timeout_experimental(timeout_ms);

        if(ch != EOF) get();

        return ch;
    }

    // timeout to be used with above timeout experimental functions
    uint16_t experimental_timeout_ms = 500;

    // NOTE: herein lies the rub of iostreams
    // peek for iostreams means block and wait for a character, but then only peek at it once it's available
    // peek for some other scenarios doesn't block, and returns EOF or similar unless data is available
    // since iostream blocks, that means peek is the "lightest" call you can make to stoke a retrieval from
    // the hardware.  since we aren't following that paradigm, we are left with nothing to stoke retrieval
    // from hardware.  For UARTs this shouldnt be an issue since external parties don't need our help to push
    // us characters, but might be an issue with other interfaces.  In any case, our version of peek() can still
    // stoke that input, but it just won't ever block
    int peek_timeout_experimental()
    { return peek_timeout_experimental(experimental_timeout_ms);}

    int get_timeout_experimental()
    { return get_timeout_experimental(); }

#ifndef FEATURE_DISCRETE_PARSER
    bool is_match(char c, const char* match)
    {
        char delim;

        while((delim = *match++))
            if(delim == c) return true;

#ifdef DEBUG_ATC_MATCH
        fstd::clog << "Didn't match on char# " << (int) c << fstd::endl;
#endif

        return false;
    }

    bool is_delimiter(char c)
    {
        return is_match(c, delimiters);
    }
#endif

    bool input_match(char match)
    {
        int ch = get();
        bool matched = ch == match;
#ifdef DEBUG_ATC_MATCH
        debug_context.identify(fstd::clog);
        fstd::clog << "Match raw '" << match << "' = " << (matched ? "true" : "false - instead got ");
        if(!matched) fstd::clog << '"' << (char)ch << '"';
        fstd::clog << fstd::endl;
#endif
        return matched;
    }

    /**
     * @brief Matches a string against input
     *
     * @param match - string to compare against input
     * @return true if successful
     */
    bool input_match(const char* match)
    {
        char ch;

#ifdef DEBUG_ATC_MATCH
        debug_context.identify(fstd::clog);
        fstd::clog << "Match raw '" << match << "' = ";
#endif

        while((ch = *match++))
        {
#ifdef FEATURE_ATC_PEEK
            char _ch = get();
            if(ch != _ch)
            {
                unget(_ch);
#else
            int _ch = peek();
            if(ch == _ch)
            {
                // TODO: use ignore() instead
                get();
            }
            else
            {
#endif
#ifdef DEBUG_ATC_MATCH
                fstd::clog << "false   preset='" << ch << "',incoming='" << _ch << '\'' << fstd::endl;
#endif
                return false;
            }
        }

#ifdef DEBUG_ATC_MATCH
        fstd::clog << "true" << fstd::endl;
#endif
        return true;
    }


    /**
     * @brief Match on one of many keywords described in an array
     *
     * @param keywords null-terminated array of possible matching keywords
     * @return pointer to matched keyword, or nullptr
     */
    const char* input_match(const char** keywords)
    {
        layer3::MultiMatcher matcher(keywords);
        int ch;

#ifdef FEATURE_ATC_PEEK
        while((ch = get()) != -1)
        {
            if(!matcher.parse(ch)) break;
        }
#else
        while((ch = peek_timeout_experimental()) != EOF)
        {
            if(!matcher.parse(ch)) break;
            get();
        }
#endif

#ifdef DEBUG_ATC_MATCH
        debug_context.identify(fstd::clog);

        if(!matcher.is_matched())
        {
            fstd::clog << "No match found" << fstd::endl;
        }
        else
        {
            fstd::clog << "Matched: " << matcher.matched() << fstd::endl;
        }
#endif

        if(matcher.is_matched())
        {
#ifdef FEATURE_ATC_PEEK
            unget(ch);
#endif
            return matcher.matched();
        }
        else
            // don't bother ungetting here because we're already in a funky state, so who cares
            return nullptr;
    }

    // FIX: Right now hard-wired to full-line input matching
    // FIX: Also preallocates big buffer for that
    const char* input_match_old(const char** keywords)
    {
        char buf[128];

        getline(buf, 128);

        const char *result = layer3::MultiMatcher::do_match(buf, keywords);
#ifdef DEBUG_ATC_INPUT
        if(result)
            fstd::clog << "Matched: " << result << fstd::endl;
        else
            fstd::clog << "Matched nothing" << fstd::endl;
#endif
        return result;
    }


    template <typename T>
    bool _input_match(T match)
    {
        constexpr uint8_t size = experimental::maxStringLength<T>();
        char buf[size + 1];

        toString(buf, match);

        return input_match(buf);
    }


    // retrieves a text string in input up to max size
    // leaves any discovered delimiter cached
    size_t input(char* input, size_t max);


    template <typename T>
    bool input(T& inputValue)
    {
        // TODO: disallow constants from coming in here
        //static_assert(T, "Cannot input into a static pointer");

        constexpr uint8_t maxlen = experimental::maxStringLength<T>();
        char buffer[maxlen];

        size_t n = input(buffer, maxlen);
#ifdef DEBUG
        const char* error = validateString<T>(buffer);
        if(error)
        {
            set_error("validation", error);
            return false;
        }

        if(n == 0) return false;
#endif
        inputValue = fromString<T>(buffer);

#ifdef DEBUG_ATC_INPUT
        fstd::clog << "Input raw = " << buffer << " / cooked = ";
        fstd::clog << inputValue << fstd::endl;
#endif

        return true;
    }

    /*
    template <typename T>
    ATCommander& operator>>(T inputValue);
    */
    /*
    // FIX: not getting picked up
    template <> template<size_t size>
    ATCommander& operator>>(char buf[size])
    {
        return *this;
    }
    */

    /*
    template <typename T>
    ATCommander& operator>>(T& inputValue)
    {
        input(inputValue);
        return *this;
    } */

    void write(const char* s, fstd::streamsize len)
    {
#ifdef DEBUG_ATC_OUTPUT
        fstd::clog.write(s, len);
#endif

        cout.write(s, len);
    }


    void put(char ch)
    {
#ifdef DEBUG_ATC_OUTPUT
        fstd::clog.put(ch);
#endif

        cout.put(ch);
    }

    template <typename T>
    ATCommander& operator<<(T outputValue)
    {
#ifdef DEBUG_ATC_OUTPUT
        fstd::clog << outputValue;
#endif
        cout << outputValue;
        return *this;
    }

    // retrieve and ignore all whitespace and newlines
    // leaves non-whitespace/newline character cached
    void ignore_whitespace_and_newlines();

    // retrieve input until it's not whitespace,
    // leaves non-whitespace character cached
    void ignore_whitespace();

    bool skip_newline();

    /// Read one character from input, expecting newline-ish characters
    /// @return if newline-ish found, return true.  Otherwise, false
    bool input_newline() { return skip_newline(); }


    bool check_for_ok();

    void send()
    {
#ifdef DEBUG_ATC_OUTPUT
        fstd::clog << fstd::endl;
#endif
        cout << fstd::endl;
    }

    // "request command" is an "ATxxxx?" with an "ATxxxx: " response
    // send a request command and parse response up until
    // non-echo portion
    void send_request(const char* cmd)
    {
        cout << AT << cmd << '?' << fstd::endl;
    }

    void recv_request_echo_prefix(const char *cmd)
    {
        ignore_whitespace_and_newlines();

        input_match(cmd);
        input_match(": ");
    }


    // sends a "request command" and parses the prefix
    // part of its response
    void do_request_prefix(const char* cmd);


    template <typename T, size_t N>
    void do_command_opt(T (&s)[N])
    {
        // TODO: do a check to grab only the right types here (const char[])
        // so that we can overload with do_command
        cout.write(AT, 2);
        cout.write(s, N);
    }

    // do a basic command with no parameters
    // no ENDL is sent
    template <typename TCmd>
    void do_command(TCmd cmd)
    {
        cout.write(AT, 2);
        cout << cmd;
    }

    // a "assign" is an "ATxxxx=" command
    // parameters are handled by << operator
    // no ENDL is sent
    void do_assign(const char* cmd);


    template <class ...TArgs>
    void send_assign(const char* cmd, TArgs...args)
    {
        //const int size = sizeof...(args);

        do_assign(cmd);
        _send(args...);
        send();
    }

    template <class TCmd, class ...TArgs>
    void send_command(TCmd cmd, TArgs...args)
    {
        do_command(cmd);
        _send(args...);
        send();
    }

    /// Send of an AT command (either assign or command mode expected)
    /// \tparam TCmdClass struct containing command definitions
    /// \tparam TArgs
    /// \param args arguments to send to TCmdClass::suffix
    template <class TCmdClass, class ...TArgs>
    void command(TArgs...args)
    {
        // for some reason, some compilers are treating "constexpr char[]" as not const in this case
#ifdef DEBUG_ATC_STRICT
        static_assert(TCmdClass::CMD, "Looking for CMD.  Are you pointing to the right class?");
#endif

        TCmdClass::command::request(*this, args...);
        TCmdClass::command::response(*this);
    }

    /// Same as command, but for times where we explicitly expect to be in ATE1 echoback mode
    /// \tparam TCmdClass
    /// \tparam TArgs
    /// \param args
    template <class TCmdClass, class ...TArgs>
    void command_with_echo(TArgs...args)
    {
        // for some reason, some compilers are treating "constexpr char[]" as not const in this case
#ifdef DEBUG_ATC_STRICT
        static_assert(TCmdClass::CMD, "Looking for CMD.  Are you pointing to the right class?");
#endif

        TCmdClass::command::request(*this, args...);
        TCmdClass::command::read_echo(*this, args...);
        TCmdClass::command::response(*this);
    }

    template <class TCmdClass, class ...TArgs>
    auto status(TArgs...args) -> decltype(TCmdClass::status::response(*this, args...))
    {
#ifdef DEBUG_ATC_STRICT
        static_assert(TCmdClass::CMD, "Looking for CMD.  Are you pointing to the right class?");
#endif

        TCmdClass::status::request(*this);
        return TCmdClass::status::response(*this, args...);
    }
};

/*
template <typename T>
inline ATCommander& operator >>(ATCommander& atc, T value)
{
    value = atc.input(value);
    return atc;
}

template <>
inline ATCommander& operator >>(ATCommander& atc, const char* value)
{
    return atc;
} */
template <typename T>
inline ATCommander& operator >>(ATCommander& atc, T value);

template <>
inline ATCommander& operator >>(ATCommander& atc, char& value)
{
    value = atc.get();
    return atc;
}

template <>
inline ATCommander& operator >>(ATCommander& atc, const char value)
{
    atc.input_match(value);
    return atc;
}


//template <>
inline ATCommander& operator >>(ATCommander& atc, uint8_t& value)
{
    atc.input(value);
    return atc;
}


//template <>
inline ATCommander& operator >>(ATCommander& atc, uint16_t& value)
{
    atc.input(value);
    return atc;
}


inline ATCommander& operator >>(ATCommander& atc, float& value)
{
    atc.input(value);
    return atc;
}


template <>
inline ATCommander& operator>>(ATCommander& atc, char* inputValue)
{
    atc.input(inputValue, 100);
    return atc;
}

template <>
inline ATCommander& operator>>(ATCommander& atc, const char* matchValue)
{
    if(!atc.input_match(matchValue))
    {
        atc.set_error("match", matchValue);
    }
    return atc;
}


// FIX: Beware, this only works properly with char* type T
// so basically broken
template <typename T>
inline ATCommander::_experimental::Formatter& operator>>(ATCommander::_experimental::Formatter& atcf, T value)
{
    atcf.atc >> value;

    if(atcf.eat_delimiter())
        atcf.atc.get();

    return atcf;
}


template <typename T>
inline ATCommander::_experimental::Formatter& operator<<(ATCommander::_experimental::Formatter& atcf, T value)
{
    if(atcf.auto_delimit())
        atcf.atc << ',';

    atcf.atc << value;
    return atcf;
}

/*
template <>
bool ATCommander::_input_match(uint8_t match)
{
    constexpr uint8_t size = experimental::maxStringLength<uint8_t>();
    char buf[size];

    return input_match(match);
}*/

/*
template <typename T>
inline ATCommander& operator >>(ATCommander& atc, T& value)
{
    atc.input(value);
    return atc;
}
*/
