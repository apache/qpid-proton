#ifndef OPTIONS_HPP
#define OPTIONS_HPP
/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <string>
#include <sstream>
#include <ostream>
#include <vector>
#include <stdexcept>

namespace example {
/** bad_option is thrown for option parsing errors */
struct bad_option : public std::runtime_error {
    bad_option(const std::string& s) : std::runtime_error(s) {}
};

/** Simple command-line option parser for example programs */
class options {
  public:

    options(int argc, char const * const * argv) : argc_(argc), argv_(argv), prog_(argv[0]), help_() {
        size_t slash = prog_.find_last_of("/\\");
        if (slash != std::string::npos)
            prog_ = prog_.substr(slash+1); // Extract prog name from path
        add_flag(help_, 'h', "help", "Print the help message");
    }

    ~options() {
        for (opts::iterator i = opts_.begin(); i != opts_.end(); ++i)
            delete *i;
    }

    /** Updates value when parse() is called if option is present with a value. */
    template<class T>
    void add_value(T& value, char short_name, const std::string& long_name, const std::string& description, const std::string var) {
        opts_.push_back(new option_value<T>(value, short_name, long_name, description, var));
    }

    /** Sets flag when parse() is called if option is present. */
    void add_flag(bool& flag, char short_name, const std::string& long_name, const std::string& description) {
        opts_.push_back(new option_flag(flag, short_name, long_name, description));
    }

    /** Parse the command line, return the index of the first non-option argument.
     *@throws bad_option if there is a parsing error or unknown option.
     */
    int parse() {
        int arg = 1;
        for (; arg < argc_ && argv_[arg][0] == '-'; ++arg) {
            opts::iterator i = opts_.begin();
            while (i != opts_.end() && !(*i)->parse(argc_, argv_, arg))
                ++i;
            if (i == opts_.end())
                throw bad_option(std::string("unknown option ") + argv_[arg]);
        }
        if (help_) throw bad_option("");
        return arg;
    }

    /** Print a usage message */
  friend std::ostream& operator<<(std::ostream& os, const options& op) {
      os << std::endl << "usage: " << op.prog_ << " [options]" << std::endl;
      os << std::endl << "options:" << std::endl;
      for (opts::const_iterator i = op.opts_.begin(); i < op.opts_.end(); ++i)
          os << **i << std::endl;
      return os;
  }

 private:
    class option {
      public:
        option(char s, const std::string& l, const std::string& d, const std::string v) :
            short_(std::string("-") + s), long_("--" + l), desc_(d), var_(v) {}
        virtual ~option() {}

        virtual bool parse(int argc, char const * const * argv, int &i) = 0;
        virtual void print_default(std::ostream&) const {}

      friend std::ostream& operator<<(std::ostream& os, const option& op) {
          os << "  " << op.short_;
          if (!op.var_.empty()) os << " " << op.var_;
          os << ", " << op.long_;
          if (!op.var_.empty()) os << "=" << op.var_;
          os << std::endl << "        " << op.desc_;
          op.print_default(os);
          return os;
      }

      protected:
        std::string short_, long_, desc_, var_;
    };

    template <class T>
    class option_value : public option {
      public:
        option_value(T& value, char s, const std::string& l, const std::string& d, const std::string& v) :
            option(s, l, d, v), value_(value) {}

        bool parse(int argc, char const * const * argv, int &i) {
            std::string arg(argv[i]);
            if (arg == short_ || arg == long_) {
                if (i < argc-1) {
                    set_value(arg, argv[++i]);
                    return true;
                } else {
                    throw bad_option("missing value for " + arg);
                }
            }
            if (arg.compare(0, long_.size(), long_) == 0 && arg[long_.size()] == '=' ) {
                set_value(long_, arg.substr(long_.size()+1));
                return true;
            }
            return false;
        }

        virtual void print_default(std::ostream& os) const { os << " (default " << value_ << ")"; }

        void set_value(const std::string& opt, const std::string& s) {
            std::istringstream is(s);
            is >> value_;
            if (is.fail() || is.bad())
                throw bad_option("bad value for " + opt + ": " + s);
        }

      private:
        T& value_;
    };

    class option_flag: public option {
      public:
        option_flag(bool& flag, const char s, const std::string& l, const std::string& d) :
            option(s, l, d, ""), flag_(flag)
        { flag_ = false; }

        bool parse(int /*argc*/, char const * const * argv, int &i) {
            if (argv[i] == short_ || argv[i] == long_) {
                flag_ = true;
                return true;
            } else {
                return false;
            }
        }

      private:
        bool &flag_;
    };

    typedef std::vector<option*> opts;

    int argc_;
    char const * const * argv_;
    std::string prog_;
    opts opts_;
    bool help_;
};
}

#endif // OPTIONS_HPP
