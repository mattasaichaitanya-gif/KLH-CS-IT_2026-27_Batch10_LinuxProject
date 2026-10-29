/**
 * @file simulation.js
 * @brief Discrete-Event Simulation Engine Modeling POSIX Semaphores & Mutex
 * POSIX Concurrency Visualizer
 */

class ProducerConsumerSimulation {
  constructor(options, onStateChange, onLog) {
    this.capacity = options.capacity || 5;
    this.numProducers = options.numProducers || 2;
    this.numConsumers = options.numConsumers || 2;
    this.totalItems = options.totalItems || 24;
    this.paceMs = options.paceMs || 500;

    this.onStateChange = onStateChange;
    this.onLog = onLog;

    this.reset();
  }

  reset() {
    this.isRunning = false;
    if (this.timerId) {
      clearTimeout(this.timerId);
      this.timerId = null;
    }

    this.buffer = {
      slots: new Array(this.capacity).fill(null),
      capacity: this.capacity,
      head: 0,
      tail: 0,
      count: 0
    };

    this.sync = {
      sem_empty: this.capacity,
      sem_full: 0,
      mutex_locked: false,
      mutex_holder: null
    };

    this.producers = [];
    for (let i = 1; i <= this.numProducers; i++) {
      this.producers.push({
        id: i,
        type: 'producer',
        state: 'READY',
        itemsGenerated: 0,
        pendingItem: null
      });
    }

    this.consumers = [];
    for (let i = 1; i <= this.numConsumers; i++) {
      this.consumers.push({
        id: i,
        type: 'consumer',
        state: 'WAITING_FULL',
        itemsConsumed: 0,
        currentItem: null
      });
    }

    this.producedCount = 0;
    this.consumedCount = 0;
    this.nextSeqId = 1;
    this.corruptedItems = 0;
    this.peakOccupancy = 0;

    this.notifyState();
  }

  notifyState(actionEvent = null) {
    if (this.onStateChange) {
      this.onStateChange({
        buffer: { ...this.buffer, slots: [...this.buffer.slots] },
        sync: { ...this.sync },
        producers: this.producers.map(p => ({ ...p })),
        consumers: this.consumers.map(c => ({ ...c })),
        producedCount: this.producedCount,
        consumedCount: this.consumedCount,
        totalItems: this.totalItems,
        corruptedItems: this.corruptedItems,
        peakOccupancy: this.peakOccupancy,
        isRunning: this.isRunning,
        actionEvent
      });
    }
  }

  log(type, msg, meta = {}) {
    if (this.onLog) {
      const now = new Date();
      const timeStr = `${String(now.getHours()).padStart(2, '0')}:${String(now.getMinutes()).padStart(2, '0')}:${String(now.getSeconds()).padStart(2, '0')}`;
      this.onLog(type, msg, { time: timeStr, ...meta });
    }
  }

  start() {
    if (this.isRunning) return;
    this.isRunning = true;
    this.log('info', `Started: ${this.numProducers}P, ${this.numConsumers}C, N=${this.capacity}`, { tag: 'SYSTEM' });
    this.notifyState();
    this.scheduleNextStep();
  }

  pause() {
    this.isRunning = false;
    if (this.timerId) {
      clearTimeout(this.timerId);
      this.timerId = null;
    }
    this.log('warn', 'Execution paused', { tag: 'SYSTEM' });
    this.notifyState();
  }

  scheduleNextStep() {
    if (!this.isRunning) return;
    this.timerId = setTimeout(() => {
      this.step();
      if (this.isRunning) {
        this.scheduleNextStep();
      }
    }, this.paceMs);
  }

  step() {
    if (this.consumedCount >= this.totalItems) {
      this.isRunning = false;
      this.log('info', `Completed: all ${this.totalItems} items processed safely`, { tag: 'DONE' });
      this.notifyState();
      return;
    }

    const candidates = [];
    this.producers.forEach(p => candidates.push(p));
    this.consumers.forEach(c => candidates.push(c));
    candidates.sort(() => Math.random() - 0.5);

    let acted = false;
    for (const thread of candidates) {
      if (thread.type === 'producer') {
        acted = this.stepProducer(thread);
      } else {
        acted = this.stepConsumer(thread);
      }
      if (acted) break;
    }

    let alertMsg = null;
    if (this.buffer.count === this.capacity && this.sync.sem_empty === 0) {
      alertMsg = { type: 'full', text: `Buffer Full: producers waiting on sem_empty` };
    } else if (this.buffer.count === 0 && this.sync.sem_full === 0 && this.producedCount < this.totalItems) {
      alertMsg = { type: 'empty', text: `Buffer Empty: consumers waiting on sem_full` };
    }

    this.notifyState(alertMsg);
  }

  stepProducer(p) {
    if (this.producedCount >= this.totalItems && p.state === 'READY') {
      p.state = 'DONE';
      return false;
    }

    if (p.state === 'READY') {
      const seq = this.nextSeqId++;
      p.pendingItem = {
        id: seq,
        producer_id: p.id,
        value: (seq * 17 + p.id * 31) % 1000,
        timestamp: Date.now()
      };
      p.state = 'WAITING_EMPTY';
      this.log('prod', `P${p.id} created #${p.pendingItem.id}`, { thread: `P${p.id}` });
      return true;
    }

    if (p.state === 'WAITING_EMPTY') {
      if (this.sync.sem_empty > 0) {
        this.sync.sem_empty--;
        p.state = 'LOCKING_MUTEX';
        this.log('prod', `P${p.id} sem_wait(empty) ok`, { thread: `P${p.id}` });
        return true;
      }
      return false;
    }

    if (p.state === 'LOCKING_MUTEX') {
      if (!this.sync.mutex_locked) {
        this.sync.mutex_locked = true;
        this.sync.mutex_holder = `P${p.id}`;
        p.state = 'IN_CRITICAL';
        this.log('sync', `P${p.id} acquired mutex`, { thread: `P${p.id}` });
        return true;
      }
      return false;
    }

    if (p.state === 'IN_CRITICAL') {
      const writeSlot = this.buffer.tail;
      this.buffer.slots[writeSlot] = p.pendingItem;
      this.buffer.tail = (this.buffer.tail + 1) % this.capacity;
      this.buffer.count++;
      if (this.buffer.count > this.peakOccupancy) {
        this.peakOccupancy = this.buffer.count;
      }

      this.producedCount++;
      p.itemsGenerated++;

      if (window.bufferVisualizer) {
        window.bufferVisualizer.triggerPulse(writeSlot, 'produce');
      }

      const itemId = p.pendingItem.id;
      p.pendingItem = null;

      this.sync.mutex_locked = false;
      this.sync.mutex_holder = null;
      this.sync.sem_full++;

      p.state = this.producedCount < this.totalItems ? 'READY' : 'DONE';
      this.log('prod', `P${p.id} wrote #${itemId} to [${writeSlot}] -> sem_post(full)`, { thread: `P${p.id}` });
      return true;
    }

    return false;
  }

  stepConsumer(c) {
    if (c.state === 'WAITING_FULL') {
      if (this.sync.sem_full > 0) {
        this.sync.sem_full--;
        c.state = 'LOCKING_MUTEX';
        this.log('cons', `C${c.id} sem_wait(full) ok`, { thread: `C${c.id}` });
        return true;
      }
      return false;
    }

    if (c.state === 'LOCKING_MUTEX') {
      if (!this.sync.mutex_locked) {
        this.sync.mutex_locked = true;
        this.sync.mutex_holder = `C${c.id}`;
        c.state = 'IN_CRITICAL';
        this.log('sync', `C${c.id} acquired mutex`, { thread: `C${c.id}` });
        return true;
      }
      return false;
    }

    if (c.state === 'IN_CRITICAL') {
      const readSlot = this.buffer.head;
      c.currentItem = this.buffer.slots[readSlot];
      this.buffer.slots[readSlot] = null;
      this.buffer.head = (this.buffer.head + 1) % this.capacity;
      this.buffer.count--;

      this.consumedCount++;
      c.itemsConsumed++;

      if (window.bufferVisualizer) {
        window.bufferVisualizer.triggerPulse(readSlot, 'consume');
      }

      const itemId = c.currentItem ? c.currentItem.id : '?';

      this.sync.mutex_locked = false;
      this.sync.mutex_holder = null;
      this.sync.sem_empty++;

      c.state = 'PROCESSING';
      this.log('cons', `C${c.id} read #${itemId} from [${readSlot}] -> sem_post(empty)`, { thread: `C${c.id}` });
      return true;
    }

    if (c.state === 'PROCESSING') {
      const itemId = c.currentItem ? c.currentItem.id : '?';
      c.currentItem = null;
      c.state = (this.producedCount >= this.totalItems && this.buffer.count === 0) ? 'DONE' : 'WAITING_FULL';
      this.log('cons', `C${c.id} verified #${itemId} checksum OK`, { thread: `C${c.id}` });
      return true;
    }

    return false;
  }
}

window.ProducerConsumerSimulation = ProducerConsumerSimulation;
