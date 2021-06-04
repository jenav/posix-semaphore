

const SemaphoreCPP = require('bindings')('addon').Semaphore
const semaphoreNames = {}

function parseOptions (options) {
  if (typeof options !== 'object') {
    options = {}
  }
  if (!(options.create === false)) {
    options.create = true
  }
  if (typeof options.mask === 'undefined' || typeof options.mask !== 'string') {
    options.mask = "0644";
  }
  if (!options.debug) {
    options.debug = false
  } else {
    options.debug = true
  }
  if (!(options.retryOnEintr === true)) {
    options.retryOnEintr = false
  }
  return options
}

function registerExitHandler (options, onExit) {
  process.on('SIGINT', () => {
    setTimeout(() => { process.exit(0) }, 4000)
    onExit()
    process.exit(0)
  })
  process.on('exit', onExit)
  process.on('uncaughtException', (err) => {
    console.error(err.stack)
    if (options.debug) {
      console.log('[posix-semaphore] Catched uncaughtException, closing semaphore if necessary...')
    }
    setTimeout(() => { process.exit(1) }, 4000)
    onExit()
    process.exit(1)
  })
}

function Semaphore(name, options) {
  if (!(this instanceof Semaphore)) {
    return new Semaphore(name, options)
  }

  if (typeof name !== 'string') {
    throw new Error('Semaphore() expects a string as first argument')
  }

  if (semaphoreNames[name] === 1) {
    throw new Error(`Semaphore "${name}" already open in this process`)
  }

  this.wait = () => {
    this.sem.wait()
  }
  
  this.tryWait = () => {
    this.sem.tryWait()
  }

  this.post = () => {
    this.sem.post()
  }

  this.close = () => {
    this.sem.close()
    delete semaphoreNames[name]
    this.closed = true
  }
  
  this.getValue = () => {
    this.sem.getValue()
  }

  this.name = name
  options = parseOptions(options)
  this.sem = new SemaphoreCPP(name, options.create, options.mask, options.debug, options.retryOnEintr, options.value)
  semaphoreNames[name] = 1
  if (options.closeOnExit === undefined || options.closeOnExit) {
    const onExit = () => {
      if (this.closed !== true) {
        if (options.debug) {
          console.log(`[posix-semaphore] Exiting, closing semaphore "${this.name}"... (to prevent this behavior, set the \'closeOnExit\' option to false)`)
        }
        this.close()
        if (options.debug) {
          console.log('[posix-semaphore] done.')
        }
      }
    }
    registerExitHandler(options, onExit)
  }
}

module.exports = Semaphore
