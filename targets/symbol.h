#pragma once

#include <cdk/types/basic_type.h>

#include <memory>
#include <string>

namespace p6 {

  class symbol {
    std::shared_ptr<cdk::basic_type> _type;
    std::string _name;
    int _offset;
    bool _is_global;
    bool _is_argument;
    bool _is_function;
    int _qualifier;
    bool _is_defined;

  public:
    symbol(std::shared_ptr<cdk::basic_type> type, const std::string &name, int offset = 0,
           bool is_global = true, bool is_argument = false, bool is_function = false,
           int qualifier = 0)
        : _type(type), _name(name), _offset(offset), _is_global(is_global),
          _is_argument(is_argument), _is_function(is_function), _qualifier(qualifier),
          _is_defined(false) {}

    virtual ~symbol() {
      // EMPTY
    }

    bool is_typed(cdk::typename_type name) const { return _type->name() == name; }

    std::shared_ptr<cdk::basic_type> type() const { return _type; }
    const std::string &name() const { return _name; }

    int offset() const { return _offset; }
    void offset(int o) { _offset = o; }

    bool is_global() const { return _is_global; }
    bool is_argument() const { return _is_argument; }
    bool is_function() const { return _is_function; }
    int qualifier() const { return _qualifier; }

    bool is_defined() const { return _is_defined; }
    void set_defined() { _is_defined = true; }
  };

} // namespace p6
