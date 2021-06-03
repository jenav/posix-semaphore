[![Build Status](https://travis-ci.org/dbousque/posix-semaphore.svg?branch=master)](https://travis-ci.org/dbousque/posix-semaphore)

# posix-semaphore

### Installation
`npm install posix-semaphore`

### Example 1
```javascript
const Semaphore = require('posix-semaphore')

const sem = new Semaphore('mySemaphore')
sem.wait()

/* my code using shared resources 😎 */

sem.post()
// other processes are now free to use the resources

// close the semaphore for the process
sem.close()

// remove the semaphore from the system
sem.unlink()
```
### Example 2
```javascript
const Semaphore = require('posix-semaphore')

const sem = new Semaphore('mySemaphore')
try {
  sem.tryWait()

  /* my code using shared resources 😎 */

  sem.post()
  // other processes are now free to use the resources
} catch (e) {
  console.error(e)
} finally {
  // close and remove the semaphore from the system
  sem.close()
  sem.unlink()
}
```

### Inter-process communication example
```javascript
const cluster = require('cluster')
const Semaphore = require('posix-semaphore')
const shm = require('shm-typed-array')

function parentProcess () {
  const semParent = new Semaphore('mySemaphore', { debug: true })
  const bufParent = shm.create(4096)
  // we get the lock
  semParent.wait()

  // we create the child process
  const child = cluster.fork({ SHM_KEY: bufParent.key })

  // we write some data to the shared memory segment
  bufParent.write('hi there :)')
  // we release the lock
  semParent.post()

  // we close the child after a second
  setTimeout(() => { child.kill('SIGINT') }, 1000)
}

function childProcess () {
  const semChild = new Semaphore('mySemaphore', { debug: true })
  const shmKey = parseInt(process.env.SHM_KEY)
  const bufChild = shm.get(shmKey)
  
  // we get the lock, will block until the parent has released
  semChild.wait()
  // should print 'hi there :)'
  console.log(bufChild.toString())
}

if (cluster.isMaster) {
  parentProcess()
} else if (cluster.isWorker) {
  childProcess()
}
```
Output:
```
$ node test.js
hi there :)
shm segments destroyed: 1
$
```

### API

#### `new Semaphore(semName, options)`

Opens a new or an already existing semaphore with `sem_open`. Fails with an error if the semaphore could not be opened or acquired.
- `semName` : name of the semaphore
- `options` :
  - `create` : If true, create the semaphore if it doesn't exist. Otherwise just try to open an already existing one. Default : true
  - `retryOnEintr` : If `sem_wait` fails with `EINTR` (usually it's due to a SIGINT signal being fired on CTRL-C), try to acquire the lock again. Default : false
  - `value` : Initial value of semaphore. Default : 1
  - `debug` : Prints useful information. Default : false
  - `closeOnExit` : If true, the semaphore will be closed on process exit (uncaughtException, SIGINT, normal exit). Default : true 

#### `sem.wait()`

The call will block until the semaphore is acquired by the process (will happen instantly if no other process acquired the lock). Calls `sem_wait` under the hood.

#### `sem.tryWait()`

The call will not block if the semaphore can't be acquired by the process. Calls `sem_trywait` under the hood.

#### `sem.post()`

Releases the semaphore if it had been acquired, allowing other processes to acquire the lock. Calls `sem_post` under the hood.

#### `sem.close()`

Closes semaphore, meaning that the current processe will no longer have access to it. Calls `sem_close` under the hood.

#### `sem.unlink()`

Unlinks the semaphore, meaning that other processes will no longer have access to it. Calls `sem_unlink` under the hood.
