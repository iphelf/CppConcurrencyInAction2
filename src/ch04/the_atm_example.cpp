//
// Created by iphelf on 2023-10-01.
//

#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

namespace messaging {

class queue {
  struct message_base {
    virtual ~message_base() = default;
  };
  friend class fallback_dispatcher;
  template <typename PreviousDispatcher, typename Msg, typename Handler>
  friend class dispatcher;
  std::mutex mutex;
  std::condition_variable cond;
  std::queue<std::unique_ptr<message_base>> q;

 public:
  template <typename Msg>
  struct message : message_base {
    Msg content;
    explicit message(const Msg& msg) : content{msg} {}
    explicit message(Msg&& msg) : content{std::move(msg)} {}
  };

  template <typename Msg>
  void push(const Msg& msg) {
    std::lock_guard lock{mutex};
    q.push(std::make_unique<message<Msg>>(msg));
    cond.notify_all();
  }

  std::unique_ptr<message_base> wait_and_pop() {
    std::unique_lock lock{mutex};
    cond.wait(lock, [this] { return !q.empty(); });
    auto msg = std::move(q.front());
    q.pop();
    return msg;
  }
};

class sender {
  queue* q{nullptr};

 public:
  sender() = default;
  explicit sender(queue* q) : q{q} {}

  template <typename Msg>
  void send(const Msg& msg) {
    if (!q) return;
    q->push(msg);
  }
};

template <typename PreviousDispatcher, typename Msg, typename Handler>
class dispatcher {
  queue* q;
  PreviousDispatcher* prev;
  Handler handler;
  bool chained{false};
  template <typename Dispatcher, typename OtherMsg, typename OtherHandler>
  friend class dispatcher;

  void wait_and_dispatch() {
    while (true) {
      auto msg{q->wait_and_pop()};
      if (dispatch(std::move(msg))) break;
    }
  }
  bool dispatch(std::unique_ptr<queue::message_base>&& msg) {
    if (queue::message<Msg> *
        message{dynamic_cast<queue::message<Msg>*>(msg.get())}) {
      handler(message->content);
      return true;
    } else
      return prev->dispatch(std::move(msg));
  }

 public:
  dispatcher(queue* q, PreviousDispatcher* prev, Handler&& handler)
      : q{q}, prev{prev}, handler{std::move(handler)} {
    prev->chained = true;
  }
  ~dispatcher() noexcept(false) {
    if (chained) return;
    wait_and_dispatch();
  }

  template <typename OtherMsg, typename OtherFunc>
  dispatcher<dispatcher, OtherMsg, OtherFunc> handle(
      OtherFunc&& other_handler) {
    return dispatcher<dispatcher, OtherMsg, OtherFunc>{
        q, this, std::forward<OtherFunc>(other_handler)};
  }
};

class queue_closed : std::exception {};

struct close_queue {};

class fallback_dispatcher {
  queue* q;
  bool chained{false};
  template <typename PreviousDispatcher, typename Msg, typename Handler>
  friend class dispatcher;

  [[noreturn]] void wait_and_dispatch() {
    while (true) {
      auto msg{q->wait_and_pop()};
      dispatch(std::move(msg));
    }
  }
  bool dispatch(std::unique_ptr<queue::message_base>&& msg) {
    if (dynamic_cast<queue::message<close_queue>*>(msg.get()))
      throw queue_closed{};
    return false;
  }

 public:
  explicit fallback_dispatcher(queue* q) : q{q} {}
  ~fallback_dispatcher() noexcept(false) {
    if (chained) return;
    wait_and_dispatch();
  }

  fallback_dispatcher(const fallback_dispatcher&) = delete;
  fallback_dispatcher& operator=(const fallback_dispatcher&) = delete;
  fallback_dispatcher(fallback_dispatcher&&) = delete;
  fallback_dispatcher& operator=(fallback_dispatcher&&) = delete;

  template <typename Msg, typename Func>
  dispatcher<fallback_dispatcher, Msg, Func> handle(Func&& f) {
    return dispatcher<fallback_dispatcher, Msg, Func>{q, this,
                                                      std::forward<Func>(f)};
  }
};

class receiver {
  queue q;

 public:
  explicit operator sender() { return sender{&q}; }
  fallback_dispatcher wait() { return fallback_dispatcher{&q}; }
};

}  // namespace messaging

namespace application {

struct withdraw {
  std::string account;
  unsigned amount;
  mutable messaging::sender atm_queue;
  withdraw(std::string const& account_, unsigned amount_,
           messaging::sender atm_queue_)
      : account(account_), amount(amount_), atm_queue(atm_queue_) {}
};
struct withdraw_ok {};
struct withdraw_denied {};
struct cancel_withdrawal {
  std::string account;
  unsigned amount;
  cancel_withdrawal(std::string const& account_, unsigned amount_)
      : account(account_), amount(amount_) {}
};
struct withdrawal_processed {
  std::string account;
  unsigned amount;
  withdrawal_processed(std::string const& account_, unsigned amount_)
      : account(account_), amount(amount_) {}
};
struct card_inserted {
  std::string account;
  explicit card_inserted(std::string const& account_) : account(account_) {}
};
struct digit_pressed {
  char digit;
  explicit digit_pressed(char digit_) : digit(digit_) {}
};
struct clear_last_pressed {};
struct eject_card {};
struct withdraw_pressed {
  unsigned amount;
  explicit withdraw_pressed(unsigned amount_) : amount(amount_) {}
};
struct cancel_pressed {};
struct issue_money {
  unsigned amount;
  issue_money(unsigned amount_) : amount(amount_) {}
};
struct verify_pin {
  std::string account;
  std::string pin;
  mutable messaging::sender atm_queue;
  verify_pin(std::string account_, std::string pin_,
             messaging::sender atm_queue_)
      : account(std::move(account_)),
        pin(std::move(pin_)),
        atm_queue(atm_queue_) {}
};
struct pin_verified {};
struct pin_incorrect {};
struct display_enter_pin {};
struct display_enter_card {};
struct display_insufficient_funds {};
struct display_withdrawal_cancelled {};
struct display_pin_incorrect_message {};
struct display_withdrawal_options {};
struct get_balance {
  std::string account;
  mutable messaging::sender atm_queue;
  get_balance(std::string const& account_, messaging::sender atm_queue_)
      : account(account_), atm_queue(atm_queue_) {}
};
struct balance {
  unsigned amount;
  explicit balance(unsigned amount_) : amount(amount_) {}
};
struct display_balance {
  unsigned amount;
  explicit display_balance(unsigned amount_) : amount(amount_) {}
};
struct balance_pressed {};

class atm {
  messaging::receiver incoming;
  messaging::sender bank;
  messaging::sender interface_hardware;
  void (atm::*state)();
  std::string account;
  unsigned withdrawal_amount;
  std::string pin;
  void process_withdrawal() {
    incoming.wait()
        .handle<withdraw_ok>([&](withdraw_ok const& msg) {
          interface_hardware.send(issue_money(withdrawal_amount));
          bank.send(withdrawal_processed(account, withdrawal_amount));
          state = &atm::done_processing;
        })
        .handle<withdraw_denied>([&](withdraw_denied const& msg) {
          interface_hardware.send(display_insufficient_funds());
          state = &atm::done_processing;
        })
        .handle<cancel_pressed>([&](cancel_pressed const& msg) {
          bank.send(cancel_withdrawal(account, withdrawal_amount));
          interface_hardware.send(display_withdrawal_cancelled());
          state = &atm::done_processing;
        });
  }
  void process_balance() {
    incoming.wait()
        .handle<balance>([&](balance const& msg) {
          interface_hardware.send(display_balance(msg.amount));
          state = &atm::wait_for_action;
        })
        .handle<cancel_pressed>(
            [&](cancel_pressed const& msg) { state = &atm::done_processing; });
  }
  void wait_for_action() {
    interface_hardware.send(display_withdrawal_options());
    incoming.wait()
        .handle<withdraw_pressed>([&](withdraw_pressed const& msg) {
          withdrawal_amount = msg.amount;
          bank.send(withdraw(account, msg.amount, messaging::sender{incoming}));
          state = &atm::process_withdrawal;
        })
        .handle<balance_pressed>([&](balance_pressed const& msg) {
          bank.send(get_balance(account, messaging::sender{incoming}));
          state = &atm::process_balance;
        })
        .handle<cancel_pressed>(
            [&](cancel_pressed const& msg) { state = &atm::done_processing; });
  }
  void verifying_pin() {
    incoming.wait()
        .handle<pin_verified>(
            [&](pin_verified const& msg) { state = &atm::wait_for_action; })
        .handle<pin_incorrect>([&](pin_incorrect const& msg) {
          interface_hardware.send(display_pin_incorrect_message());
          state = &atm::done_processing;
        })
        .handle<cancel_pressed>(
            [&](cancel_pressed const& msg) { state = &atm::done_processing; });
  }
  void getting_pin() {
    incoming.wait()
        .handle<digit_pressed>([&](digit_pressed const& msg) {
          unsigned const pin_length = 4;
          pin += msg.digit;
          if (pin.length() == pin_length) {
            bank.send(verify_pin(account, pin, messaging::sender{incoming}));
            state = &atm::verifying_pin;
          }
        })
        .handle<clear_last_pressed>([&](clear_last_pressed const& msg) {
          if (!pin.empty()) {
            pin.pop_back();
          }
        })
        .handle<cancel_pressed>(
            [&](cancel_pressed const& msg) { state = &atm::done_processing; });
  }
  void waiting_for_card() {
    interface_hardware.send(display_enter_card());
    incoming.wait().handle<card_inserted>([&](card_inserted const& msg) {
      account = msg.account;
      pin = "";
      interface_hardware.send(display_enter_pin());
      state = &atm::getting_pin;
    });
  }
  void done_processing() {
    interface_hardware.send(eject_card());
    state = &atm::waiting_for_card;
  }
  atm(atm const&) = delete;
  atm& operator=(atm const&) = delete;

 public:
  atm(messaging::sender bank_, messaging::sender interface_hardware_)
      : bank(bank_), interface_hardware(interface_hardware_) {}
  void done() { get_sender().send(messaging::close_queue()); }
  void run() {
    state = &atm::waiting_for_card;
    try {
      for (;;) {
        (this->*state)();
      }
    } catch (messaging::queue_closed const&) {
    }
  }
  messaging::sender get_sender() { return messaging::sender{incoming}; }
};

class bank_machine {
  messaging::receiver incoming;
  unsigned balance_amount;

 public:
  bank_machine() : balance_amount(199) {}
  void done() { get_sender().send(messaging::close_queue()); }
  void run() {
    try {
      for (;;) {
        incoming.wait()
            .handle<verify_pin>([&](verify_pin const& msg) {
              if (msg.pin == "1937") {
                msg.atm_queue.send(pin_verified());
              } else {
                msg.atm_queue.send(pin_incorrect());
              }
            })
            .handle<withdraw>([&](withdraw const& msg) {
              if (balance_amount >= msg.amount) {
                msg.atm_queue.send(withdraw_ok());
                balance_amount -= msg.amount;
              } else {
                msg.atm_queue.send(withdraw_denied());
              }
            })
            .handle<get_balance>([&](get_balance const& msg) {
              msg.atm_queue.send(balance(balance_amount));
            })
            .handle<withdrawal_processed>(
                [&](withdrawal_processed const& msg) {})
            .handle<cancel_withdrawal>([&](cancel_withdrawal const& msg) {});
      }
    } catch (messaging::queue_closed const&) {
    }
  }
  messaging::sender get_sender() { return messaging::sender{incoming}; }
};

class interface_machine {
  messaging::receiver incoming;
  std::mutex iom;

 public:
  void done() { get_sender().send(messaging::close_queue()); }
  void run() {
    try {
      for (;;) {
        incoming.wait()
            .handle<issue_money>([&](issue_money const& msg) {
              {
                std::lock_guard<std::mutex> lk(iom);
                std::cout << "Issuing " << msg.amount << std::endl;
              }
            })
            .handle<display_insufficient_funds>(
                [&](display_insufficient_funds const& msg) {
                  {
                    std::lock_guard<std::mutex> lk(iom);
                    std::cout << "Insufficient funds" << std::endl;
                  }
                })
            .handle<display_enter_pin>([&](display_enter_pin const& msg) {
              {
                std::lock_guard<std::mutex> lk(iom);
                std::cout << "Please enter your PIN (0-9)" << std::endl;
              }
            })
            .handle<display_enter_card>([&](display_enter_card const& msg) {
              {
                std::lock_guard<std::mutex> lk(iom);
                std::cout << "Please enter your card (I)" << std::endl;
              }
            })
            .handle<display_balance>([&](display_balance const& msg) {
              {
                std::lock_guard<std::mutex> lk(iom);
                std::cout << "The balance of your account is " << msg.amount
                          << std::endl;
              }
            })
            .handle<display_withdrawal_options>(
                [&](display_withdrawal_options const& msg) {
                  {
                    std::lock_guard<std::mutex> lk(iom);
                    std::cout << "Withdraw 50? (w)" << std::endl;
                    std::cout << "Display Balance? (b)" << std::endl;
                    std::cout << "Cancel? (c)" << std::endl;
                  }
                })
            .handle<display_withdrawal_cancelled>(
                [&](display_withdrawal_cancelled const& msg) {
                  {
                    std::lock_guard<std::mutex> lk(iom);
                    std::cout << "Withdrawal cancelled" << std::endl;
                  }
                })
            .handle<display_pin_incorrect_message>(
                [&](display_pin_incorrect_message const& msg) {
                  {
                    std::lock_guard<std::mutex> lk(iom);
                    std::cout << "PIN incorrect" << std::endl;
                  }
                })
            .handle<eject_card>([&](eject_card const& msg) {
              {
                std::lock_guard<std::mutex> lk(iom);
                std::cout << "Ejecting card" << std::endl;
              }
            });
      }
    } catch (messaging::queue_closed&) {
    }
  }
  messaging::sender get_sender() { return messaging::sender{incoming}; }
};

}  // namespace application

int main() {
  using namespace application;
  bank_machine bank;
  interface_machine interface_hardware;
  atm machine(bank.get_sender(), interface_hardware.get_sender());
  std::thread bank_thread(&bank_machine::run, &bank);
  std::thread if_thread(&interface_machine::run, &interface_hardware);
  std::thread atm_thread(&atm::run, &machine);
  messaging::sender atmqueue(machine.get_sender());
  bool quit_pressed = false;
  while (!quit_pressed) {
    char c = static_cast<char>(getchar());
    switch (c) {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        atmqueue.send(digit_pressed(c));
        break;
      case 'b':
        atmqueue.send(balance_pressed());
        break;
      case 'w':
        atmqueue.send(withdraw_pressed(50));
        break;
      case 'c':
        atmqueue.send(cancel_pressed());
        break;
      case 'q':
        quit_pressed = true;
        break;
      case 'i':
        atmqueue.send(card_inserted("acc1234"));
        break;
      default:
        break;
    }
  }
  bank.done();
  machine.done();
  interface_hardware.done();
  atm_thread.join();
  bank_thread.join();
  if_thread.join();
}