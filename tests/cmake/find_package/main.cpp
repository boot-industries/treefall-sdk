#include <orpheus/abi.h>

int main() {
  const auto *session = orpheus_session_abi_v1();
  return session == nullptr;
}
