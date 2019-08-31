#define runChainPOSTCHAIN                                                      \
  for (auto blk : chain->blocks) {                                             \
    if (blk->postChain) {                                                      \
      try {                                                                    \
        blk->postChain(blk, context);                                          \
      } catch (boost::context::detail::forced_unwind const &e) {               \
        throw;                                                                 \
      } catch (const std::exception &e) {                                      \
        LOG(ERROR) << "Post chain failure, failed block: "                     \
                   << std::string(blk->name(blk));                             \
        if (context->error.length() > 0)                                       \
          LOG(ERROR) << "Last error: " << std::string(context->error);         \
        LOG(ERROR) << e.what() << "\n";                                        \
        return {chain->previousOutput, Failed};                                       \
      } catch (...) {                                                          \
        LOG(ERROR) << "Post chain failure, failed block: "                     \
                   << std::string(blk->name(blk));                             \
        if (context->error.length() > 0)                                       \
          LOG(ERROR) << "Last error: " << std::string(context->error);         \
        return {chain->previousOutput, Failed};                                       \
      }                                                                        \
    }                                                                          \
  }
