#pragma once
#include <vector>


/// A State-based inter-thread communication library.
///
/// A single `Producer` can send a new `State` object to multiple `Channels`, which in
/// turn pass it on to multiple `Consumers` which receive the state on some `Bus`
///
/// State is intended to be little more than a simple `POD`-style struct, and it is up
/// to the individual consumers to determine which members have actually changed if they
/// require so.
namespace otto::itc {

  /// The concept that state types need to fulfill.
  template<typename State>
  concept AState = std::is_copy_constructible_v<State>;

  // Forward Declarations

  /// A Channel through which a single Producer can send state to multiple consumers
  template<AState State>
  struct Channel;

  /// Sends state to multiple Consumers through a Channel
  template<AState State>
  struct Producer;

  /// Receives state from a single producer through a Channel
  template<AState State>
  struct Consumer;

  // Declarations

  template<AState State>
  struct Channel {
    using Consumer = Consumer<State>;
    using Producer = Producer<State>;

    Channel() noexcept {}
    Channel(const Channel&) = delete;
    ~Channel() noexcept
    {
      for (auto* c : consumers_) {
        c->channel_ = nullptr;
      }
      if (producer_) producer_->internal_remove_channel(*this);
    }

    const std::vector<Consumer*>& consumers() const noexcept
    {
      return consumers_;
    }

    Producer* producer() const noexcept
    {
      return producer_;
    }

    /// Set the producer for this channel
    ///
    /// Can be `nullptr` to clear the current producer
    void set_producer(Producer* p) noexcept
    {
      producer_ = p;
      if (p) p->channels_.emplace_back(this);
    }

    /// Set the producer for this channel
    void set_producer(Producer& p) noexcept
    {
      set_producer(&p);
    }

  protected:
  private:
    friend Producer;
    friend Consumer;

    /// Only called from Consumer constructor
    void internal_add_consumer(Consumer* c)
    {
      consumers_.push_back(c);
    }

    ///
    void internal_produce(const State& s)
    {
      for (auto* cons : consumers_) cons->internal_produce(s);
    }

    std::vector<Consumer*> consumers_;
    Producer* producer_ = nullptr;
  };

  template<AState State>
  struct Producer {
    using Channel = Channel<State>;
    Producer(Channel& ch)
    {
      ch.set_producer(this);
    }

    ~Producer() noexcept
    {
      for (auto* ch : channels_) {
        ch->set_producer(nullptr);
      }
    }

    /// The channels this producer is currently linked to
    const std::vector<Channel*>& channels() const noexcept
    {
      return channels_;
    }

  protected:
    void produce(const State& s)
    {
      for (auto* chan : channels_) {
        chan->internal_produce(s);
      }
    }

  private:
    friend Channel;

    /// Called only from set_producer in Channel
    void internal_add_channel(Channel& ch)
    {
      channels_.push_back(ch);
    }

    /// Called only from Channel destructor
    void internal_remove_channel(Channel& ch)
    {
      std::erase(channels_, &ch);
    }

    std::vector<Channel*> channels_;
  };

  template<AState State>
  struct Consumer {
    using Channel = Channel<State>;

    Consumer(Channel& ch) : channel_(&ch)
    {
      ch.internal_add_consumer(this);
    }

    virtual ~Consumer() noexcept
    {
      if (channel_) std::erase(channel_->consumers_, this);
    }

    /// The channel this consumer is registered on
    Channel* channel() const noexcept
    {
      return channel_;
    }

  protected:
    /// Access the newest state available.
    const State& state() const noexcept
    {
      return state_;
    }

    /// Hook called with the new state right before the state is updated
    ///
    /// In this hook, the old state is available through the {@ref state()} member function
    /// Override in subclass if needed
    virtual void on_new_state(const State& s) {}

  private:
    friend Channel;

    /// Called from {@ref Channel::internal_produce}
    void internal_produce(const State& s)
    {
      on_new_state(s);
      state_ = s;
    }

    Channel* channel_;
    State state_;
  };

} // namespace otto::itc
