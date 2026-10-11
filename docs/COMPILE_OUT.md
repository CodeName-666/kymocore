# Compile-time configuration and compile-out

KymoCore resolves each build setting in this order: compiler `-D` definitions,
the optional header named by `KYMO_USER_CONFIG_HEADER`, then the defaults in
`src/kymo_build_config.h`. A project header should use `#ifndef` around every
setting so a build profile can override it. Pass the include token as a quoted
macro value, for example `-DKYMO_USER_CONFIG_HEADER=\"kymo_user_config.h\"`.

`KYMO_ENABLED` defaults to `1`. Set it to `0` to remove the library
implementations and callable low-level API declarations during preprocessing.
The setting does not remove application statements or objects that were written
outside Kymo guards. After migrating those call sites, profile changes require
no source edits. Every boolean setting must be the literal token `0` or `1`.

The existing settings keep their defaults: runtime is on; X, Z, timestamp, CRC,
decoder and the optional C++ sender are off. Trace, trace ISR, scheduler and
flow settings default off. `KYMO_EFFECTIVE_*` macros express what is compiled;
requested `KYMO_ENABLE_*` values remain intact when `KYMO_ENABLED=0`.
Scheduler tracing requires ISR tracing, and ISR tracing requires trace.

Use `kymo_instrumentation.h` for trace call sites. With trace disabled, all
`KYMO_TRACE_*` macros become valid no-op statements that discard their
arguments, including in C99 and in `if/else` statements. With trace enabled,
the facade includes `kymo_trace.h` and forwards to the trace API.
`KYMO_TRACE_SCOPE` is a C++ RAII scope and is independent of the optional
`KYMO_ENABLE_CPP` telemetry sender.

Every translation unit in an application and its library build must use the
same resolved configuration. `kymo_version.h` publishes
`KYMO_CONFIG_API_VERSION=1` so projects can reject a stale core that would
otherwise ignore `KYMO_ENABLED`.
