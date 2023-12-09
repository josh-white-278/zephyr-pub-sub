# Zephyr Publish Subscribe Messaging Framework

A publish subscribe message passing framework built on top of Zephyr RTOS. It provides zero copy
message passing with reference counted message allocation. It also supports publishing statically
allocated messages and publishing messages from ISRs. Messages are received by subscribers in the
order published and three different types of subscriber message queuing are supported.

## Overview

### Message Flow

The normal message flow is:

1. The publisher allocates a new message from the broker
2. The publisher populates the message with the data it wants to send
3. The publisher publishes the message to the broker
4. If a subscriber has a subscription it receives the message in its message handler function
5. After all of the subscribers have processed the message in their handler functions the message is
   released back to its allocator

### Key Concepts

Each message is given a 16 bit identifier when it is allocated (or initialized in the case of static
messages). A subscriber subscribes to the message identifiers it wants to receive and the broker
routes messages to the subscribers based on the published message identifer and the subscriber's
subscriptions.

When a publisher allocates a message it acquires a reference to the message. The publisher must
either publish the message to the broker thereby transferring ownership of its message reference to
the broker or release the message returning it unpublished to the broker. After a publisher has
published a message it must not modify the message as ownership has been transferred.

When a subscriber receives a message it is const i.e. read only, the subscriber should not modify
any received messages as they are shared pointers with other subscribers.

The broker does not transfer ownership of a message reference to the subscriber. If a subscriber
wishes to retain a reference to a received message it must acquire one before the handler function
returns. The acquired reference must be released back to the broker before the message can be
re-used. If the aquired reference is dropped without being released then the message will leak and
it is likely the broker will run out of messages to allocate.

### Initialization Order

1. Broker initialized
2. Allocators initialized
3. Allocators added to broker
4. Subscribers initialized
5. Subscribers configured
6. Subscribers added to broker

## Broker

A broker is responsible for managing message allocation and message routing. It maintains a list of
subscribers in order of priority. When a message is received on its publish queue it iterates
through the list checking each subscriber's subscriptions. If a subscription is found the message is
passed to the subscriber through its selected queuing mechanism. The broker maintains acquiring and
releasing references to messages as required, subscribers should not have to worry about it unless
they are manually acquiring additional references.

Messages allocated from a broker must be published or released back to the same broker, this is due
to how messages track which allocator they belong to. Even if two brokers share an allocator an
allocated message can only be used with the broker it was allocated from. A static message can be
published to any broker and can even be published to multiple brokers at the same time. If a static
message is to be published to multiple brokers then an additional reference must be acquired for
each additional broker before publishing the message to any broker. This is because publishing a
message transfers ownership of a reference so the correct number of references must be owned before
publishing can start.

### Default Broker

A default broker is provided for convenience, it can be disabled with
`CONFIG_PUB_SUB_DEFAULT_BROKER=n`.

## Subscribers

There are three different types of subscriber:

* Callback
* Message queue
* FIFO

Although each type receives its messages slightly different they all receive them through a handler
function. In the case of the callback subscriber the handler is called directly by the broker. The
message queue and FIFO subscribers need to call a polling function from their own threads. The
polling function manages dequeuing messages, calling the handler function and releasing
message references as required. In general a message handler function should not block but if care
is taken as to the type of subscriber (FIFO), its priority and its subscriptions it might be
possible to run blocking operations in some cases.

Each subscriber maintains a subscriptions bit-array which indicates which message identifiers the
subscriber has subscribed to. This bit-array is provided to the subscriber at initialization time
and must be sized correctly to prevent buffer overruns. The size is based on the maximum message
identifier that will be published as each bit represents a subscription to a message identifier
value.

A subscriber can only be added to a single broker. Once a subscriber is added to a broker it will
begin to receive the messages it has subscribed to. If a subscriber does not want to miss any
messages it should be added to the broker and its subscriptions set during the initialization phase
prior to any messages being published. Before a subscriber is added to a broker its message handler
must be set and, if required, its priority value.

A subscriber's priority value is used to sort the subscriber relative to other subscribers of the
same type in the broker's list of subscribers. This allows fine-grained control of the order that
the broker publishes messages to its subscribers. A subscriber's priority value is only checked when
it is added to the broker so updating the priority after being added will only take effect if the
subscriber is removed and then added back to the broker.

### Callback subscriber details

The callback subscriber is the highest priority type and all callback subscribers will receive a
message before any other type. The callback subscriber type has its message handler function called
directly from the broker's message processing thread. This means that the handler function can not
block as it will block all other subscribers from receiving messages.

### Message queue subscriber details

The message queue subscriber is the second highest priority type and all message queue subscribers
will receive a message before any other type other than the higher priority callback type. The
message queue subscriber has a fixed length message queue for receiving published messages. If the
queue is not long enough or is not serviced fast enough then it will block the broker's message
processing thread until space becomes available in the message queue.

### FIFO subscriber details

The FIFO subscriber is the lowest priority type and all other subscriber types will receive a
message before a FIFO type. The FIFO subscriber can not block the broker's message processing thread
however a published message can only be queued on a single FIFO so a high priority FIFO subscriber
can block a message from reaching a lower priority FIFO subscriber if it does not process its queued
messages fast enough. Also if the FIFO subscribers are not prioritized correctly then there could be
needless thread context switching if a high priority subscriber is running on a low priority thread.

## Messages

A publish subscribe message consists of a 2 word header (8 bytes on a 32 bit architecture) followed
by a variable number of message bytes. The header contains a pointer reserved for FIFO operations
and an atomic variable that is split into three parts:

* 16 bit message identifier
* 8 bit allocator identifier
* 8 bit reference counter

In general access to messages is provided by a `void *` pointer that points at the message bytes of
the message. Access to the message header values is provided via functions that operation on the
`void *` message pointer.

### Message allocation

Messages are allocated from a broker and are allocated based on size. Internally the broker
maintains a sorted list of allocators and iterates through them until it finds one that can allocate
a message large enough. All allocators should be added to a broker during initialization before any
message have been allocated. Once a message has been allocated from a broker additional allocators
must not be added to the broker. This is because each message tracks its allocator via its index in
the broker's list of allocators. If an allocator is added it could be inserted into the list and any
allocated messages could then have the incorrect index and be unable to be freed.

#### Supported allocator backends

* Memory slab

### Static messages

Statically allocated message can be sent through a broker provided it has reserved memory for the
message header. Prior to being published the message must first be initialized and then it must be
reset prior to every subsequent publish. When using static messages care must be taken by the
publisher not to re-use the static message until it is certain that it has been fully handled by all
of its subscribers. For regular static messages the reference counter should be checked, when it
reaches zero the publisher can re-use it.

### Callback static messages

A callback static message is a static message with an additional callback function. All of the above
caveats about static messages apply. When the callback message's reference counter reaches 0 the
callback is called to indicate the message is now free to be re-used. The callback is called from
the context of the last reference holder to release the message so care must be taken not to block
within the callback.

## TODO List

* Sample app
* Publish to subscriber directly
* Timer/delayable messages
* Heap message allocator
* Ability to run the broker publish handling on a thread or a different work queue
* Different subscriber types other than bitmask, could be a callback
* Different publish queuing mechanism other than FIFO, could be msgq or direct
* Linker section subscribers + macros for static init of run time subscribers
* Configurable msgq subscriber behavior: drop message when msgq full based on (msg_id > DROP_LEVEL),
  a per subscriber setting
