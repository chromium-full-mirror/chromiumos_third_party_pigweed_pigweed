.. _module-pw_log_tokenized:

----------------
pw_log_tokenized
----------------
.. pigweed-module::
   :name: pw_log_tokenized

The ``pw_log_tokenized`` module contains utilities for tokenized logging. It
connects ``pw_log`` to ``pw_tokenizer`` and supports
:ref:`module-pw_log-tokenized-args`.

C++ backend
===========
``pw_log_tokenized`` provides two backends for ``pw_log`` that tokenize log
messages with the ``pw_tokenizer`` module. One backend includes metadata, and
the other omits it to save code size.

With metadata
-------------
The default ``//pw_log_tokenized`` backend passes the log level, 16-bit
tokenized module name, and flag bits through a metadata argument. The macro
eventually passes logs to the :cc:`pw_log_tokenized_HandleLog` function, which
must be implemented by the application.

Example implementation:

.. code-block:: cpp

   extern "C" void pw_log_tokenized_HandleLog(uint32_t metadata,
                                              const uint8_t message[],
                                              size_t size) {
     // The metadata object provides the log level, module token, and flags.
     // These values can be recorded and used for runtime filtering.
     pw::log_tokenized::Metadata info(metadata);

     if (info.level() < current_log_level) {
       return;
     }

     if (info.flags() & HIGH_PRIORITY_LOG != 0) {
       EmitHighPriorityLog(info.module(), message, size);
     } else {
       EmitLowPriorityLog(info.module(), message, size);
     }
   }

Without metadata
----------------
The ``//pw_log_tokenized:light`` backend omits the metadata entirely to
save code size. The macro eventually passes logs to the
:cc:`pw_log_tokenized_HandleLogWithoutMetadata` function, which must be
implemented by the application.

Example implementation:

.. code-block:: cpp

   extern "C" void pw_log_tokenized_HandleLogWithoutMetadata(
       const uint8_t message[], size_t size) {
     // Since no metadata is provided, applications might route these logs
     // to a default stream without filtering.
     EmitLog(message, size);
   }

See the documentation for :ref:`module-pw_tokenizer` for further details.

Metadata in the format string
-----------------------------
With tokenized logging, the log format string is converted to a 32-bit token.
Regardless of how long the format string is, it's always represented by a 32-bit
token. Because of this, metadata can be packed into the tokenized string with
no cost.

``pw_log_tokenized`` uses a simple key-value format to encode metadata in a
format string. Each field starts with the ``■`` (U+25A0 "Black Square")
character, followed by the key name, the ``♦`` (U+2666 "Black Diamond Suit")
character, and then the value. The string is encoded as UTF-8. Key names are
comprised of alphanumeric ASCII characters and underscore and start with a
letter.

.. code-block::

   "■key1♦contents1■key2♦contents2■key3♦contents3"

This format makes the message easily machine parseable and human readable. It is
extremely unlikely to conflict with log message contents due to the characters
used.

``pw_log_tokenized`` uses three fields: ``msg``, ``module``, and ``file``.
Implementations may add other fields, but they will be ignored by the
``pw_log_tokenized`` tooling.

.. code-block::

   "■msg♦Hyperdrive %d set to %f■module♦engine■file♦propulsion/hyper.cc"

Using key-value pairs allows placing the fields in any order.
``pw_log_tokenized`` places the message first. This is prefered when tokenizing
C code because the tokenizer only hashes a fixed number of characters. If the
file were first, the long path might take most of the hashed characters,
increasing the odds of a collision with other strings in that file. In C++, all
characters in the string are hashed, so the order is not important.

The format string is created by the :cc:`PW_LOG_TOKENIZED_FORMAT_STRING`
macro.

The metadata bit field
----------------------
``pw_log_tokenized`` packs runtime-accessible metadata into a 32-bit integer
which is passed as the metadata argument for ``pw_log_tokenizer``'s global
handler with metadata facade. Packing this metadata into a single word rather
than separate arguments reduces the code size significantly.

Four items are packed into the metadata argument (when used):

- Log level -- Used for runtime log filtering by level.
- Line number -- Used to track where a log message originated.
- Log flags -- Implementation-defined log flags.
- Tokenized :c:macro:`PW_LOG_MODULE_NAME` -- Used for runtime log filtering by
  module.

For applications that do not need log metadata at runtime, ``pw_log_tokenized``
provides a macro that omits the metadata to save code size:
:cc:`PW_LOG_TOKENIZED_TO_GLOBAL_HANDLER` (used by the
``//pw_log_tokenized:light`` backend).  If this macro is used as the
``pw_log`` backend, the log is routed to the
:cc:`pw_log_tokenized_HandleLogWithoutMetadata` function, bypassing the typical
log handler.

Configuring metadata bit fields
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
The number of bits to use for each metadata field is configurable through macros
in ``pw_log/config.h``. The field widths must sum to 32 bits. A field with zero
bits allocated is excluded from the log metadata.

* :cc:`PW_LOG_TOKENIZED_LEVEL_BITS`
* :cc:`PW_LOG_TOKENIZED_LINE_BITS`
* :cc:`PW_LOG_TOKENIZED_FLAG_BITS`
* :cc:`PW_LOG_TOKENIZED_MODULE_BITS`

Creating and reading Metadata
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
``pw_log_tokenized`` provides :cc:`GenericMetadata
<pw::log_tokenized::GenericMetadata>` to facilitate the creation and
interpretation of packed log :cc:`Metadata <pw::log_tokenized::Metadata>`.

The following example shows that a ``Metadata`` object can be created from a
``uint32_t`` log metadata.

.. code-block:: cpp

   extern "C" void pw_log_tokenized_HandleLog(uint32_t metadata,
                                              const uint8_t message[],
                                              size_t size_bytes) {
     pw::log_tokenized::Metadata info = metadata;
     // Check the log level to see if this log is a crash.
     if (info.level() == PW_LOG_LEVEL_FATAL) {
       HandleCrash(info,
                   pw::ConstByteSpan(reinterpret_cast<const std::byte*>(message),
                                     size_bytes));
       PW_UNREACHABLE;
     }
     // ...
   }

It's also possible to get a ``uint32_t`` representation of a ``Metadata``
object:

.. code-block:: cpp

   // Logs an explicitly created string token.
   void LogToken(uint32_t token, int level, int line_number, int module) {
     const uint32_t metadata =
         log_tokenized::Metadata(level, module, PW_LOG_FLAGS, line_number).value();
     std::array<std::byte, sizeof(token)> token_buffer =
         pw::bytes::CopyInOrder(endian::little, token);

     pw_log_tokenized_HandleLog(
         metadata,
         reinterpret_cast<const uint8_t*>(token_buffer.data()),
         token_buffer.size());
   }

The binary tokenized message may be encoded in the :ref:`prefixed Base64 format
<module-pw_tokenizer-base64-format>` with the
:cc:`pw::log_tokenized::PrefixedBase64Encode` function.

Parsing metadata fields
-----------------------
The metadata fields packed into the format string can be parsed with the
:cc:`pw::log_tokenized::ParseFields` function. This function takes a string
and a callback that is called for each key-value pair.

Build targets
-------------
The build for ``pw_log_tokenized`` provides two backend targets for the
``pw_log`` facade: ``//pw_log_tokenized`` and
``//pw_log_tokenized:light``. Both targets provide the
``pw_log_tokenized/log_tokenized.h`` header.

- ``//pw_log_tokenized`` routes logs to the ``pw_log_tokenized:handler`` facade, which must
  be implemented by the user.
- ``//pw_log_tokenized:light`` routes logs to the ``pw_log_tokenized:light_handler``
  facade, which must also be implemented by the user.

GCC has a bug resulting in section attributes of templated functions being
ignored. This in turn means that log tokenization cannot work for templated
functions, because the token database entries are lost at build time.
For more information see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=70435.
If you are using GCC, the ``gcc_partially_tokenized`` target can be used as a
backend for the ``pw_log`` facade instead which tokenizes as much as possible
and uses the ``pw_log_string:handler`` for the rest using string logging.

Python package
==============
``pw_log_tokenized`` includes a Python package for decoding tokenized logs.

pw_log_tokenized
----------------
.. automodule:: pw_log_tokenized
  :members:
  :undoc-members:
