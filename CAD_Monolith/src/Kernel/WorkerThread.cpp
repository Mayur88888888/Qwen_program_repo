#include "Kernel/WorkerThread.h"

namespace Cad {

// Implementation of lock-free message queue helper methods

bool KernelMessageQueue::try_enqueue(const KernelMessage& msg) {
    return m_queue.try_enqueue(msg);
}

bool KernelMessageQueue::try_dequeue(KernelMessage& msg) {
    return m_queue.try_dequeue(msg);
}

size_t KernelMessageQueue::size() const {
    return m_queue.size_approx();
}

bool KernelMessageQueue::empty() const {
    return m_queue.empty();
}

void KernelMessageQueue::clear() {
    KernelMessage dummy;
    while (m_queue.try_dequeue(dummy)) {}
}

} // namespace Cad
