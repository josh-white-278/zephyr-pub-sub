# Zephyr Publish Subscribe Messaging Framework

A publish subscribe message passing framework built on top of Zephyr RTOS. It provides zero copy
message passing with reference counted message allocation. It also supports publishing statically
allocated messages and publishing messages from ISRs.

## [Tutorials](doc/tutorials/tutorials.md)

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
message returning it unpublished to its allocator. Additionally, due to the use of FIFOs for queuing
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

## Broker

A broker is responsible for managing message routing and acquiring/releasing message references for
its subscribers. It maintains a list of subscribers in order of priority. When a message is received
on its publish queue it iterates through the list checking each subscriber's subscriptions. If a
subscription is found the message is passed to the subscriber. The broker acquires and releases
references to messages as required, subscribers should not have to worry about it unless they are
manually acquiring additional references. Messages can only be published to a single broker due to
the FIFO message queuing mechanism used by the broker.

### Default Broker

A default broker is provided for convenience, it can be disabled with
`CONFIG_PUB_SUB_DEFAULT_BROKER=n`.

## Subscribers

A subscriber receives any messages published to it through its message handler function where the
handler function is always called from the subscriber's work queue thread. In general a subscriber
should try to avoid blocking operations in the handler function as it can block lower priority
subscribers from receiving messages that both are subscribed to. This is due to the internal FIFO
message queuing which only allows a message to be queued with a single subscriber at once. As long
as care is taken as to the priority of each subscriber this limitation should (hopefully) not
overally constrain an application.

Each subscriber maintains a subscriptions bit-array which indicates which message identifiers the
subscriber has subscribed to. This bit-array is provided to the subscriber at initialization time
and must be sized correctly to prevent buffer overruns. The size is based on the maximum public
message identifier that will be published as each bit represents a subscription to a message
identifier value.

A subscriber can only be added to a single broker. Once a subscriber is added to a broker it will
begin to receive the messages it has subscribed to. If a subscriber does not want to miss any
messages it should be added to the broker and its subscriptions set during the initialization phase
prior to any messages being published.

A subscriber's priority value is used to sort the subscriber relative to other subscribers in the
broker's list of subscribers. This allows fine-grained control of the order that the broker
publishes messages to its subscribers. The priority value selected for a subscriber should be
aligned with the subscriber's work queue priority i.e. a high priority subscriber should not be run
on a low priority work queue. A subscriber's priority value is only checked when it is added to the
broker so updating the priority after being added will only take affect if the subscriber is removed
and then added back to the broker.

## Messages

A publish subscribe message consists of a 2 word header (8 bytes on a 32 bit architecture) followed
by a variable number of message bytes. The header contains a pointer reserved for FIFO operations
and an atomic variable that is split into three parts:

* 16 bit message identifier
* 8 bit allocator identifier
* 8 bit reference counter

In general access to messages is provided by a `void *` pointer that points at the message bytes of
the message. Access to the message header values is provided via functions that operation on the
`void *` message pointer e.g. `uint16_t pub_sub_msg_get_msg_id(const void *msg)`.

### Message allocation

Messages are allocated from an allocator and track which allocator they belong to via an allocator
id. Internally a list of allocators is maintained and the allocator id provides the index into the
list for the allocator. A message allocator can be defined statically, for example using
`PUB_SUB_MEM_SLAB_ALLOCATOR_DEFINE_STATIC`. Statically defined allocators use a linker section to
create the list of allocators for tracking purposes. Runtime allocators can also be used with the
caveat that the allocator must be added to the runtime list of allocators before any messages are
allocated from it. Adding the allocator assigns it an allocator id and without a valid allocator id
messages can not be released back to the correct allocator.

#### Message allocator pool

Message allocators can be combined into an allocator pool. When an message is allocated from a pool
the size of the message is used to select which allocator the message is allocated from i.e. the
pool selects the allocator with the smallest maximum message size that can accommodate the new
message. An allocator pool makes it more ergonomic to share allocators across an application at the
cost of slightly less efficient message allocation.

#### Supported allocator backends

* Memory slab

### Static messages

Statically allocated messages can be sent through a broker provided it has reserved memory for the
message header. A static messsage must be initialized before being published for the first time to
ensure it has its message header values set correctly. Additionally a reference to the message must
be aquired prior to every publish to ensure that the message's reference counter is one when it is
published. When using static messages care must be taken by the publisher not to re-use the static
message until it is certain that it has been fully handled by all of its subscribers. For regular
static messages the reference counter should be checked, when it reaches zero the publisher can
re-use it.

### Callback static messages

A callback static message is a static message with an additional callback function. All of the above
caveats about static messages apply. When the callback message's reference counter reaches 0 the
callback is called to indicate the message is now free to be re-used. The callback is called from
the context of the last reference holder to release the message so care must be taken with the
operations performed within the callback.

### Delayable messages

A delayable message is a static message that is scheduled to be published in the future. A delayable
message is published directly to a subscriber and so must have a private message id. The amount of
time delayed is at least as much time as specified but could be greater depending on how fast the
subscriber is processing its messages, the configured tick granularity etc.

If a delayable message is aborted there is a chance that it is already in the subscriber's message
queue and will still be received by the subscriber after the abort. Similarly for updating the
timeout, if the message has already timed out but has not been processed by the subscriber then
it will be received twice. The first for the already queued timeout and then second after the
updated timeout expires. To handle both of these cases delayable messages have an internal flag to
track whether they have been aborted or not and the subscriber can check the aborted state of the
message when it is handled. The aborted flag is automatically cleared after the message has been
handled.

One caveat with the aborted flag is that it will get set if the message is started from the
subscriber's message handler when it is handling the message. This is because the message's
reference counter is used to determine if the message is queued or not. The message is not released
by the framework until after the message handler returns so the reference counter is always non-zero
while the message is being handled. Therefore if a delayable message's aborted flag is checked it
should always be checked first to ensure its state is correct. E.g.:

``` C
static void msg_handler(uint16_t msg_id, const void *msg, void *user_data)
{
    switch (msg_id) {
    case MSG_ID_DELAYABLE_MSG: {
        struct pub_sub_delayable_msg *delayable_msg =
            PUB_SUB_MSG_TO_DELAYABLE_MSG(struct pub_sub_delayable_msg, msg);
        if (!pub_sub_delayable_msg_was_aborted(delayable_msg)) {
           // When the message is started here the message's reference counter is
            // non-zero which means the message's aborted flag will be set
           pub_sub_delayable_msg_start(delayable_msg, K_MSEC(500));
           // pub_sub_delayable_msg_was_aborted(delayable_msg) will now return
           // true until the msg_handler returns and the message is released.
       }
       break;
    }
    }
};
```

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
* HSM documentation
* Heap message allocator
