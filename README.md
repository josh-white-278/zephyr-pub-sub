# Zephyr Publish Subscribe Messaging Framework

A publish subscribe message passing framework built on top of Zephyr RTOS. It provides zero copy
message passing with reference counted message allocation. It also supports publishing statically
allocated messages and publishing messages from ISRs. Messages are received by subscribers in the
order published and three different types of subscriber message queuing are supported.

## Overview

### Message Flow

The normal message flow is:

1. The publisher allocates a new message
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

Each subscriber splits the message id space into two, public message ids and private message ids.
Public messages must be published to a broker and must be subscribed to by a subscriber to be
received. Public message definitions are shared across subscribers and can be received by any
subscriber on a broker. Private message ids are not subscribed to by a subscriber and are published
directly to a subscriber. Private message are defined by the individual subscriber, a private
message with the same id can have a completely different message definition between two different
subscribers.

When a publisher allocates a message it acquires a reference to the message. The publisher must
either publish the message thereby transferring ownership of its message reference or release the
message returning it unpublished to its allocator. Additionally due to the use of FIFOs for queuing
messages a message must only be published once even if more than one reference to the message is
owned.

After a publisher has published a message it must not modify the message as ownership has been
transferred. When a subscriber receives a message it is const i.e. read only, the subscriber should
not modify any received messages as they are shared pointers with other subscribers. Private
messages may be an exception to this rule as there will be only one subscriber receiving the
message, however, it will depend on an application's specific use case.

The broker does not transfer ownership of a message reference to the subscriber. If a subscriber
wishes to retain a reference to a received message it must acquire one before the handler function
returns. The acquired reference must be released before the message can be re-used. If the aquired
reference is dropped without being released then the message will leak and it is likely the
allocator will run out of messages to allocate.

### Initialization Order

1. Broker initialized
2. Runtime allocators initialized
3. Runtime allocators added
4. Subscribers initialized
5. Subscribers configured
6. Subscribers added to broker

## Broker

A broker is responsible for managing message routing and acquiring/releasing message references for
its subscribers. It maintains a list of subscribers in order of priority. When a message is received
on its publish queue it iterates through the list checking each subscriber's subscriptions. If a
subscription is found the message is passed to the subscriber through its selected queuing
mechanism. The broker acquires and releases references to messages as required, subscribers should
not have to worry about it unless they are manually acquiring additional references. Messages can
only be published to a single broker due to the FIFO message queuing mechanism used by the broker.

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

Messages are allocated from an allocator and track which allocator they belong to via an allocator
id. Internally a list of allocators is maintained and the allocator id provides the index into the
list for the allocator. A message allocator can be defined statically, for example using
`PUB_SUB_MEM_SLAB_ALLOCATOR_DEFINE_STATIC`. Statically defined allocators use a linker section to
create the list of allocators for tracking purposes. Runtime allocators can also be used with the
caveat that the allocator must be added to the runtime list of allocators before any messages are
allocated from it. Adding the allocator assigns it an allocator id and without a valid allocator id
messages can not be released back to the correct allocator.

#### Supported allocator backends

* Memory slab

### Static messages

Statically allocated message can be sent through a broker provided it has reserved memory for the
message header. Prior to being published the message must first be initialized and then it must be
aquired prior to every publish. When using static messages care must be taken by the publisher not
to re-use the static message until it is certain that it has been fully handled by all of its
subscribers. For regular static messages the reference counter should be checked, when it reaches
zero the publisher can re-use it.

### Callback static messages

A callback static message is a static message with an additional callback function. All of the above
caveats about static messages apply. When the callback message's reference counter reaches 0 the
callback is called to indicate the message is now free to be re-used. The callback is called from
the context of the last reference holder to release the message so care must be taken not to block
within the callback.

### Delayable messages

A delayable message is a static message that is scheduled to be published in the future. A delayable
message is only published directly to a subscriber and so must have a private message id. The amount
of time delayed is at least as much time as specified but could be greater depending on how fast a
subscriber is processing its messages, the configured tick granularity etc. If high precision timing
with low jitter is required by an application a delayable message will probably not be the tool for
the job.

A subscriber must process a delayable message before it times out a second time e.g. if a delayable
message is queued with a subscriber and the message's timeout is restarted, the subscriber must
process the queued message before the message's timeout triggers and attempts to queue the message
a second time. The safest way to ensure this is to only start a delayable message from the
subscriber when it is handling the delayable message itself. Depending on what the delayable message
is being used for this may not be possible and, if so, care must be taken to ensure that the delays
used are aligned to the general responsiveness of the subscriber i.e. the delays used are longer
than the worse case subscriber message handling time.

If a delayable message is aborted there is a chance that it is already in the subscriber's message
queue and will still be received by the subscriber after the abort. Similarly for updating the
timeout, if the message has already timed out but has not been processed by the subscriber then
it may look like the message has timed out immediately as the subscriber will receive the message
twice. Additionally updating or aborting a delayable message from a thread that is not the
subscriber's thread has further edge cases as the subscriber may be processing the delayable message
when the message is being updated/aborted from the other thread. Adding a cancelled/updated flag to
the message and wrapping message accesses with a mutex in the multi-threaded case may be sufficient
to mitigate these edge cases depending on the application's use case.

## Additional Notes

### Peer to peer messages

An application may choose to split the message id space into three: public message ids, peer to peer
message ids and private message ids. The functionality of peer to peer messages sits between public
and private messages. Peer to peer messages are published to a single subscriber like a private
message but they have shared message definitions and message ids like a public message. This concept
allows things like many to one request/response messaging to be handled within the framework without
having to add unique identifiers and filtering to the requests and responses. Additionally it can be
used to reduce the size of an application's subscribers' subscription arrays and the message
processing overhead for messages that are only received by a single subscriber.

### TODO List

* Sample app
* Heap message allocator
* Ability to run the broker publish handling on a thread or a different work queue
* Different subscriber types other than bitmask, could be a callback
* Different publish queuing mechanism other than FIFO, could be msgq or direct
* Linker section subscribers + macros for static init of run time subscribers
* Configurable msgq subscriber behavior: drop message when msgq full based on (msg_id > DROP_LEVEL),
  a per subscriber setting
