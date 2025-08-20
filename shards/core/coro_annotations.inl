namespace shards {
SHARDS_COND_INLINE void coroResumed(SHContext *context) {
  SHWire *wire = context->currentWire();
  if (!wire)
    return;

#if SH_DEBUG_THREAD_NAMES
  shards::pushThreadName(wire->threadNameStrings.init(wire).resumeStr);
#endif

#ifdef SH_VERBOSE_COROUTINES_LOGGING
  SHLOG_TRACE("> Resumed wire {}", wire->name);
#endif

#if SH_DEBUG
  shassert(!context->isResumed);
  context->isResumed = true;
#endif

  auto &logTs = shards::logging::ThreadState::get();
  if (context->linkedLogContext) {
    // Push thread logging state
    auto prevContext = logTs.current;
    std::swap(context->prevLogContext, logTs.current);
    // Reattach the parent log context, in case we are stepping from somewhere else
    context->linkedLogContext->linkRootTo(prevContext);
  } else {
    // Push thread logging state
    std::swap(context->prevLogContext, logTs.current);
  }
}

SHARDS_COND_INLINE void coroSuspended(SHContext *context) {
  SHWire *wire = context->currentWire();
  if (!wire)
    return;

#if SH_DEBUG
  shassert(context->isResumed);
  context->isResumed = false;
#endif

#if SH_DEBUG_THREAD_NAMES
  shards::popThreadName();
#endif

#ifdef SH_VERBOSE_COROUTINES_LOGGING
  SHLOG_TRACE("< Suspended wire {}", wire->name);
#endif

  auto &logTs = shards::logging::ThreadState::get();
  if (context->linkedLogContext) {
    shassert(context->prevLogContext != &*context->linkedLogContext && "Prev log context should not be linked log context");
    context->linkedLogContext->unlink();
  }
  std::swap(context->prevLogContext, logTs.current);
}

SHARDS_COND_INLINE void coroExtResume(SHWire *wire) {
  if (!wire)
    return;

#if SH_DEBUG_THREAD_NAMES
  shards::pushThreadName(wire->threadNameStrings.init(wire).extResumeStr);
#endif

  TracyCoroEnter(wire);

#ifdef SH_VERBOSE_COROUTINES_LOGGING
  SHLOG_TRACE("Resuming wire {}", wire->name);
#endif
}

SHARDS_COND_INLINE void coroExtSuspend(SHWire *wire) {
  if (!wire)
    return;

#if SH_DEBUG_THREAD_NAMES
  shards::popThreadName();
#endif

  TracyCoroExit(wire);

#ifdef SH_VERBOSE_COROUTINES_LOGGING
  SHLOG_TRACE("Suspending wire {}", wire->name);
#endif
}
} // namespace shards