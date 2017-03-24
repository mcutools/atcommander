#pragma once

#include "atcommander.h"

class ATBuilder
{
protected:
    template <typename TResponse>
    struct status_helper_autoresponse
    {
        static TResponse response_suffix(ATCommander& atc)
        {
            TResponse input;

            atc.input(input);

            return input;
        }
    };

    template <class ...TArgs>
    struct command_helper_autorequest
    {

        static void suffix(ATCommander& atc, TArgs...args)
        {
            atc._send(args...);
        }

    };


    struct _command_base
    {
        template <typename T, size_t N>
        static void prefix(ATCommander& atc, T (&s)[N])
        {
            atc.cout.write(ATCommander::AT, 2);
            atc.cout.write(s, N - 1);
        }

        static void prefix(ATCommander& atc, char c)
        {
            atc.cout.write(ATCommander::AT, 2);
            atc.cout.put(c);
        }
    };

public:
    //typedef ATCommander::_command_base command_base;
    template <class TProvider, class TMethodProvider = TProvider>
    struct command : _command_base
    {
        static void prefix(ATCommander& atc)
        {
            _command_base::prefix(atc, TProvider::CMD);
        }

        // Default response behavior is only to check for OK after a command
        // oftentimes this is NOT what you want, so be sure to overload (override-ish)
        static bool response(ATCommander& atc)
        {
            return atc.check_for_ok();
        }

        template <class ...TArgs>
        static void request(ATCommander& atc, TArgs...args)
        {
            prefix(atc);
            TMethodProvider::suffix(atc, args...);
            atc.send();
        }


        /*
        void response(ATCommander &atc)
        {
            atc.check_for_ok();
        } */

        /*
        // as discussed http://stackoverflow.com/questions/257288/is-it-possible-to-write-a-template-to-check-for-a-functions-existence
        // and here http://stackoverflow.com/questions/87372/check-if-a-class-has-a-member-function-of-a-given-signature
        template <typename T>
        struct func_exists
        {
            typedef char one;
            typedef long two;

            template <typename C> static one test( typeof(&C::response) ) ;
            template <typename C> static two test(...);

            enum { value = sizeof(test<T>(0)) == sizeof(char) };
        }; */

        // Works, but don't need it - overloading does the trick
        template<typename T>
        struct HasUsedMemoryMethod
        {
            template<typename U, size_t (U::*)() const> struct SFINAE {};
            template<typename U> static char Test(SFINAE<U, &U::response>*);
            template<typename U> static int Test(...);
            static const bool Has = sizeof(Test<T>(0)) == sizeof(char);
        };

        // TODO: be mindful, this might be a C++14 only feature
        template <class ...TArgs>
        //static auto response(ATCommander& atc, TArgs...args) -> decltype(TMethodProvider::response(atc, args...))
        static void response(ATCommander& atc, TArgs...args)
        {
            //auto returnValue = TMethodProvider::response(atc, args...);
            //return returnValue;
            TMethodProvider::response(atc, args...);
        }

        template <class ...TArgs>
        static void run(ATCommander& atc, TArgs...args)
        {
            request(atc, args...);
            response(atc);
        }
    };


    template <class TProvider, class ...TArgs>
    struct command_auto : public command<TProvider, command_helper_autorequest<TArgs...>> {};


    template <const char cmd, class ...TArgs>
    struct one_shot
    {
        static constexpr char CMD = cmd;

        typedef command_auto<one_shot<cmd, TArgs...>> command;
    };


    template <class TProvider, class TMethodProvider = TProvider>
    struct assign : command<TProvider, TMethodProvider>
    {
        template <class ...TArgs>
        static void request(ATCommander& atc, TArgs...args)
        {
            command<TProvider>::prefix(atc);
            atc.cout.put('=');
            TMethodProvider::suffix(atc, args...);
            atc.send();
        }

        template <class ...TArgs>
        static void run(ATCommander& atc, TArgs...args)
        {
            request(atc, args...);
            command<TProvider, TMethodProvider>::response(atc);
        }
    };


    template <class TProvider, class TAssignParameter>
    struct assign_auto : assign<TProvider, command_helper_autorequest<TAssignParameter>> {};

    template <class TProvider, class TMethodProvider = TProvider>
    struct status : _command_base
    {
        static void request(ATCommander& atc)
        {
            _command_base::prefix(atc, TProvider::CMD);
            atc.cout.put('?');
            atc.send();
        }

        static void response_prefix(ATCommander& atc)
        {
            atc.input_match(TProvider::CMD);
            atc.input_match(": ");
        }

        // TODO: be mindful, this might be a C++14 only feature
        template <class ...TArgs>
        static auto response(ATCommander& atc, TArgs...args) -> decltype(TMethodProvider::response_suffix(atc, args...))
        //static void response(ATCommander& atc, TArgs...args)
        {
            response_prefix(atc);
            auto returnValue = TMethodProvider::response_suffix(atc, args...);
            return returnValue;
        }

        template <class ...TArgs>
        static void run(ATCommander& atc, TArgs...args)
        {
            request(atc);
            response(atc, args...);
        }
    };

    template <class TProvider, class TReturnType>
    struct status_auto : public status<TProvider, status_helper_autoresponse<TReturnType>> {};

    /*
    template <char const &array[N], int N>
    struct command_base
    {

    }; */

public:

};
