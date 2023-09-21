# Project 1 : Design Report

Team7 컴퓨터공학과 20210741 김소현 컴퓨터공학과 20210774 김주은

---

# **1. Analysis of synchronization**

### **A. Sychronization이란?**

Synchronization은 critical resource를 공유하는 2개 이상의 thread들의 concurrent execution을 말한다. 여러 thread들 간에 critical resource를 사용할 때 conflict를 방지하려면 thread들이 synchronize되어야 한다. 

### **B. Meaning and their implementation in pintos**

### **1) Semaphore**

- meaning
    
    Semaphore은 2개 이상의 thread가 shared resource나 critical section 등에 접근할 때 여러 process 혹은 thread가 접근하는 것을 막아준다. 
    
- implementation
    - **struct**
        
        ```c
        /* A counting semaphore. */
        struct semaphore 
          {
            unsigned value;             /* Current value. */
            struct list waiters;        /* List of waiting threads. */
          };
        ```
        
        `value` : semaphore의 현재 상태 : 사용 가능한 공유 자원의 개수를 나타내는 변수
        
        `waiter` : semaphore을 기다리는 thread들의 list : 공유 자원을 사용하기 위해 대기하는 waiters의 list
        
    - **function**
        - **void sema_init (struct semaphore *, unsigned value);**
        
        ```c
        void
        sema_init (struct semaphore *sema, unsigned value) 
        {
          ASSERT (sema != NULL);
        
          sema->value = value;
          list_init (&sema->waiters);
        }
        ```
        
        semaphore가 사용 가능하다면 1 이상이, 그렇지 않다면 0이 value로 assign될 것이다.
        
        공유자원을 사용하기 위해 여러 thread가 sema_down되어 대기하고 있고, 공유자원을 사용하던 thread가 사용한 후 sema_up할 때 어떤 thread가 가장 먼저 공유자원을 사용할 것인가에 대한 문제를 해결하는 것이 scheduling의 key point이다.
        
        - **void sema_down (struct semaphore *);**
        
        ```c
        void
        sema_down (struct semaphore *sema) 
        {
          enum intr_level old_level; // 현재 interrupt 상태를 저장할 변수 
        														// 나중에 interrupt 상태를 복원하기 위해 사용함
        
          ASSERT (sema != NULL); // sema가 null이면 실행 중지
          ASSERT (!intr_context ()); // 현재 code가 interrupt handler 내에서 실행되지 않는지 확인
          // it must not be called within an interrupt handler.
        
          old_level = intr_disable (); // 현재 interrupt 상태를 old_level에 저장하고, 
        															//모든 interrupt 비활성화
          
          while (sema->value == 0) // sema가 사용불가능한 상태
            {
              list_push_back (&sema->waiters, &thread_current ()->elem); 
        			// 현재 thread를 semaphore의 waiters에 추가
              thread_block ();
            }
          sema->value--; // 1만큼 감소 -> 공유 자원을 하나 사용하겠다는 의미
          intr_set_level (old_level);
        }
        ```
        
        sema_down은 running중인 thread가 semaphore을 요청할 때 사용하는 함수다. 해당 함수는 semaphore가 사용이 가능한지 여부에 상관없이 일단 실행된다. 
        
        semaphore의 value가 0(사용불가능한 상태)이라면 waiters list에 들어가고 block 상태가 된다.
        
        → block 상태가 되면 thread는 thread_block()에서 멈추며 semaphore의 value가 positive가 될 때까지 기다리는 것이다. 이후에 sema_up()이 호출되면 thread는 thread_block()부터 시작하게 된다.
        
        - **bool sema_try_down (struct semaphore *);**
        
        ```c
        bool
        sema_try_down (struct semaphore *sema) 
        {
          enum intr_level old_level; // 현재 interrupt 상태를 저장할 변수
          bool success; // semaphore 성공 여부 나타낼 변수 선언
        
          ASSERT (sema != NULL); // sema가 NULL이면 실행 중지
        
          old_level = intr_disable (); // 현재 interrupt 상태를 old_level에 저장하고, 
        															// 모든 interrupt 비활성화
        
          if (sema->value > 0) // semaphore이 positive인지 확인 (사용가능)
            {
              sema->value--; // 참이면 semaphore 감소
              success = true; // 성공
            }
          else
            success = false; // 실패 
          intr_set_level (old_level); // interrupt 상태 복원
        
          return success;
        }
        ```
        
        sema_down과 기능은 비슷하나 semaphore의 값이 0이면 즉시 반환하며 이때 성공여부를 반환한다.
        
        semaphore의 값이 positive이면 값을 1 감소시키고 true를 반환하며 0인 경우에는 block되지 않고 false를 바로 반환한다.
        
        - **void sema_up (struct semaphore *);**
        
        ```c
        /* This function may be called from an interrupt handler. */
        void
        sema_up (struct semaphore *sema) 
        {
          enum intr_level old_level; // 현재 interrupt 상태를 저장할 변수
        
          ASSERT (sema != NULL); // sema가 NULL이면 실행 중지
        
          old_level = intr_disable (); // 현재 interrupt 상태를 old_level에 저장하고, 
        															// 모든 interrupt 비활성화
        
          if (!list_empty (&sema->waiters)) // semaphore의 waiter list가 비어있지 않은지 확인
            thread_unblock (list_entry (list_pop_front (&sema->waiters),
                                    struct thread, elem)); // 비어있지 않다면 thread_unblock 실행
          // list_pop_front(&sema->waiters):semaphore의 waiter list의 첫번째 thread를 제거하고 반환
          // list_entry(..., struct thread, elem):list에서 반환된 element를 thread 구조체로 변환
          // thread_unblock : 반환된 thread를 unblock
        
          sema->value++; // semaphore 값 1 증가
          intr_set_level (old_level); // interrupt 상태 복원
        }
        ```
        
        대기 중인 thread가 waiter list에 있으면 첫번째 thread를 깨워서 실행 가능하게 하며, semaphore 값을 1만큼 증가시킴으로써 사용 가능한 공유 자원을 하나 증가시켜주는 함수다.
        
        - **void sema_self_test (void);**
        
        ```c
        void
        sema_self_test (void) 
        {
          struct semaphore sema[2]; // 2개의 semaphore 객체를 저장할 배열
          int i;
        
          printf ("Testing semaphores...");
          // 2개의 semaphore 객체 초기값 0으로 설정
          sema_init (&sema[0], 0);
          sema_init (&sema[1], 0);
          // thread_create 함수 호출 
          // 1. sema-test라는 이름으로 새로운 thread를 생성한다.
          // 2. 새로운 thread에서 sema_test_helper라는 함수가 실행되도록 한다. 
          // 2. sema_test_helper 함수의 인자로 sema 배열의 주소를 전달
          thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
          
          for (i = 0; i < 10; i++) // 총 10번의 반복문 실행
            {
              sema_up (&sema[0]); // 첫번째 semaphore을 1씩 증가
              sema_down (&sema[1]); // 두번째 semaphore을 1씩 감소
            }
          printf ("done.\n");
        }
        ```
        
        2개의 semaphore을 초기화하고 새로운 thread를 생성하여 sema_test_helper을 실행한다. 이후 첫번째 semaphore을 1만큼 증가시키고 두번째 semaphore을 1만큼 감소시키는 과정을 10번 반복한 후 test를 종료한다.
        
        - **static void sema_test_helper (void *sema_);**
        
        ```
        static void // static function : 해당 소스 파일 내에서만 접근 가능
        sema_test_helper (void *sema_) // 포인터 -> semaphore 배열을 가리키는 것
        {
          struct semaphore *sema = sema_; // semaphore의 포인터 타입으로 type casting
          int i;
          
          // sema_self_test 함수와 반대로 up down 연산 수행
          for (i = 0; i < 10; i++) 
            {
              sema_down (&sema[0]);
              sema_up (&sema[1]);
            }
        }
        ```
        
        sema_self_test와는 정반대로 up down 연산을 수행한다.
        
        - 결국 sema_self_test와 sema_test_helper함수를 같이 분석해보면
            1. 메인 thread인 sema_self_test에서 2개의 semaphore을 0으로 초기화한다.
            2. 메인 thread(sema_self_test)에서 새로운 thread를 생성하고 sema_test_helper을 호출한다.
            3. 메인 thread에서는 sema[0]에 대해 sema_up 연산을 10번 수행하여 value가 10이 된다.
            4. 이후 sema[1]에 대해 sema_down 연산을 10번 수행하는데 이 때 이 연산은 sema[1]의 값이 0이기 때문에 메인 thread를 block 상태로 만들고, sema[1]이 positive가 되도록 기다린다.
            5. sema_test_helper 함수가 실행되는 새로운 thread에서는 sema[0]에 대해 sema_down 연산을 10번 수행한다.
            6. 메인 thread에서는 sema_up으로 sema[0]을 10으로 만들어놨기 때문에 새로운 thread는 sema_down을 10번 수행해도 모두 성공적으로 수행된다.
            7. sema[1]에서 sema_up 연산이 수행하면서 값이 positive가 되기 때문에 main thread에서 대기 중이던 sema_down 연산이 수행된다.
            
            이러한 방식으로 thread의 기본적인 synchronization mechanism을 확인한다.
            
        

### **2) Lock**

- meaning
    
    synchronization mechanism 중 하나로 여러 thread나 process가 동시에 공유 자원에 접근할 때 발생할 수 있는 **race condition을 방지**하기 위해 사용된다. thread가 lock을 획득하면 해당 thread만이 그 lock이 보호하고 있는 resource나 code section에 접근할수 있게 되며 다른 thread들은 해당 lock이 release될 때 까지 대기해야 한다. 
    
    semaphore와 비슷하게 동기화 문제를 해결하기 위한 도구이지만 사용 방식이나 구현 방식이 다르다.
    
    - **한 번에 하나의 thread만**이 공유 자원 or critical section에 접근할 수 있으며, 다른 thread들은 해당 lock이 release될 때까지 대기해야 한다.
    - semaphore의 경우 특정 thread가 semaphore을 내릴수도 있고 다른 thread는 그것을 올릴 수 있다. 그러나 lock의 경우 lock을 요청한 thread가 lock을 보유하며, 해제할 수 있는 것도 lock을 획득한 thread만 가능하다.
    
- implementation
    - **struct**
    
    ```c
    /* Lock. */
    struct lock 
      {
        struct thread *holder;      /* Thread holding lock (for debugging). */
        struct semaphore semaphore; /* Binary semaphore controlling access. */
      };
    ```
    
    `holder` : lock을 보유하고 있는 thread의 pointer을 저장한다. 이는 debugging을 위해 사용될 수 있는데 어떤 thread가 lock을 보유하고 있는지, lock을 해제하지 않고 끝난 thread가 있는지 등을 파악할 수 있다.
    
    `semaphore` : 여기서는 semaphore가 binary semaphore로 사용되며 0 또는 1의 값을 가진다. thread가 lock을 획득하려고 할 때, semaphore의 값이 0, 1 중 어떤 값이냐에 따라서 thread가 대기하거나 lock을 획득하게 된다.
    
    - function
        - **void lock_init (struct lock *);**
        
        ```c
        void
        lock_init (struct lock *lock)
        {
          ASSERT (lock != NULL); // lock이 NULL인지 확인
        
          lock->holder = NULL; 
          sema_init (&lock->semaphore, 1); // sema_init 함수를 호출하여 semaphore의 초기값을 1로 설정
          // 1로 초기화 == 해당 lock이 사용 가능한 상태
        }
        ```
        
        lock 객체의 holder을 NULL로 설정하여 lock을 보유하고 있는 thread가 없음을 나타내고, semaphore을 1로 초기화하여 lock이 사용 가능한 상태임을 나타낸다.
        
        - **void lock_acquire (struct lock *);**
        
        ```c
        void
        lock_acquire (struct lock *lock)
        {
          ASSERT (lock != NULL); // lock 포인터 NULL인지 확인
          ASSERT (!intr_context ()); // lock 획득하는 것은 interrupt contexts 내에서 실행되면 안됨
        	  // lock_acquire 함수는 interrupt handler내에서 호출되어서는 안된다는 의미
          ASSERT (!lock_held_by_current_thread (lock)); 
        		// 현재 thread가 해당 lock을 보유하고 있는지 확인
            // lock은 이미 보유하고 있는데 한번 더 획득하려는 재진입성을 허용하지 않는다
        
          sema_down (&lock->semaphore); 
        		// sema_down을 호출하여 1 감소, 값이 0일 시 lock이 사용 가능해질 때까지 대기
          
        	lock->holder = thread_current (); // lock을 성공적으로 획득했으면 해당 line이 실행됨 
        	  // lock의 holder에 current thread를 설정하여 이 thread가 해당 lock을 보유하고 있음을 나타냄
        }
        ```
        
        lock_acquire 함수는 주어진 lock을 현재 thread가 획득하도록 시도하며, 획득에 성공할 경우 해당 thread를 lock holder로 나타낸다. thread_current는 실행중인 thread를 리턴하는 함수로, 해당 thread가 lock을 보유하고 있음을 struct의 holer 변수에 저장한다. 
        
        - **bool lock_try_acquire (struct lock *);**
        
        ```c
        bool
        lock_try_acquire (struct lock *lock)
        {
          bool success; // lock acquire의 성공 여부를 저장하기 위한 boolean 변수
        
          ASSERT (lock != NULL); // lock 포인터가 NULL인지 확인
          ASSERT (!lock_held_by_current_thread (lock)); 
        	// 현재 thread가 이미 lock을 보유하고 있는지 확인
          // 이미 보유하고 있다면 프로그램 수행 중지
        
          success = sema_try_down (&lock->semaphore); 
        	// sema_try_down 함수를 호출하여 lock의 semaphore 값을 1 감소
          // semaphore의 값이 0이면 실패하며 성공여부는 success에 저장된다
        
          if (success) // semaphore의 값을 감소시키는데 성공 == lock을 획득
            lock->holder = thread_current (); // 해당 lock의 holde에 현재 thread를 할당
          return success; // 성공여부 반환
        }
        ```
        
        lock을 획득하려고 시도하는 함수다. 하지만 다른 thread가 이미 lock을 보유하고 있으면 대기하지 않고 실패(success 여부)를 바로 반환한다.
        
        - **void lock_release (struct lock *);**
        
        ```c
        void
        lock_release (struct lock *lock) 
        {
          ASSERT (lock != NULL); // lock pointer가 NULL인지 확인
          ASSERT (lock_held_by_current_thread (lock)); 
        	// 현재 thread가 lock을 실제로 보유하고 있는지 확인
          // 보유하고 있지 않으면 프로그램 실행 중지
        
          lock->holder = NULL; // lock을 해제하므로 해당 lock의 holder을 NULL로 설정하여 
        											 // 현재 해당 lock을 보유하고 있는 thread가 없음을 나타내기
          sema_up (&lock->semaphore); // lock의 semaphore을 1 증가 
        															// -> 다른 대기 중인 thread가 lock을 획득할 기회를 얻음
        }
        ```
        
        - **bool lock_held_by_current_thread (const struct lock *);**
        
        ```c
        bool
        lock_held_by_current_thread (const struct lock *lock) 
        {
          ASSERT (lock != NULL); // lock pointer가 NULL인지 확인
        
          return lock->holder == thread_current (); 
        	// 현재 실행중인 thread가 lock의 holder와 일치하는지 True/False
        }
        ```
        
        - **One semaphore in a list**
        
        ```c
        struct semaphore_elem 
          {
            struct list_elem elem;              /* List element. */
            struct semaphore semaphore;         /* This semaphore. */
          };
        ```
        
        `struct list_elem` : 다양한 data type의 정보를 갖는 구조체를 linked list에 포함시키기 위해 만들어진 것으로 linked list에 사용되는 기본적인 요소다.
        
        → 결국 semaphore_elem은 linked list의 한 element로 동작하며, 이 요소는 기본 element +  semaphore가 한 쌍으로 한 element를 구성하게 한다.
        

### **3) Condition Variable**

- meaning
    
    condition variable은 synchronization의 또 다른 형태로 주로 **thread들이 어떤 조건이 충족될 때까지 대기**하는 데 사용된다. condition variable은 주로 lock과 함께 사용되며 thread는 특정 조건의 발생을 기다리면서 해당 lock을 잠시 풀어주어 다른 thread가 해당 조건을 만족시킬 수 있도록 한다.
    
    즉 condition variable은 한 쪽 코드에서 조건을 신호로 보내게 하고, 다른 cooperating 코드가 해당 신호를 받아 그에 따라 동작한다.
    
- implementation
    - **struct**
    
    ```c
    /* Condition variable. */
    struct condition 
      {
        struct list waiters;        /* List of waiting threads. */
      };
    ```
    
    `waiters` : 대기 중인 thread들의 목록
    
    - 특정 조건에 대해 대기 중인 thread들은 해당 list에 추가되며, 조건이 충족될 때 해당 thread들은 list에서 제거되어 실행을 계속하게 된다.
    
    - **function**
        - **void cond_init (struct condition *);**
        
        ```c
        void
        cond_init (struct condition *cond)
        {
          ASSERT (cond != NULL); // cond 포인터가 NULL이 아닌지 확인
        
          list_init (&cond->waiters); // waiters(대기 중인 thread들의 list)를 초기화
        }
        ```
        
        - **void cond_wait (struct condition *, struct lock *);**
        
        ```c
        void
        cond_wait (struct condition *cond, struct lock *lock) 
        {
          struct semaphore_elem waiter; // waiters(대기 list)에 추가될 semaphore element
          
          // cond, lock이 NULL이 아닌지 확인
          ASSERT (cond != NULL);
          ASSERT (lock != NULL);
          
        	// interrupt context 내에서 호출되지 않았는지 확인
          ASSERT (!intr_context ()); // interrupt handler 내에서 호출되면 안됨
          
        	ASSERT (lock_held_by_current_thread (lock)); 
        	// 현재 thread가 주어진 lock을 보유하고 있는지 확인
          // cond_wait 함수 호출 전 lock을 반드시 획득하여야 함
          
          sema_init (&waiter.semaphore, 0); // waiter의 semaphore을 초기화하고 초기값은 0
          // 이는 thread가 이 semaphore에 대기하면 바로 block될 것임을 의미
          list_push_back (&cond->waiters, &waiter.elem); // waiter을 waiters(대기 list)에 추가
          
        	lock_release (lock); // lock을 해제 -> 다른 thread가 lock획득하고 조건 변경 가능
        	 
        	sema_down (&waiter.semaphore); // waiter의 semaphore에 대기 
        		// cond_signal or cond_broadcast 함수에 의해 신호가 보내질 때까지 현재 thread를 block
          
        	lock_acquire (lock); // lock을 다시 획득 -> 결국 이 함수는 lock을 보유한 상태로 반환
        }
        ```
        
        주어진 conditional variable에 대해 대기하는 동안 관련 lock을 잠시 해제하고 나중에 다시 획득하는 역할을 한다. 이러한 동작을 통해 다른 thread가 해당 lock을 획득하고 조건 변수를 변경할 수 있게 해준다.
        
        - **void cond_signal (struct condition *, struct lock *);**
        
        ```c
        void
        cond_signal (struct condition *cond, struct lock *lock UNUSED) 
        		// UNUSED -> 함수 내부에서 사용하지 않지만 함수 호출 전에 해당 락이 보유되어 있는지를 검증함
        {
        		// cond, lock이 NULL이 아닌지 확인
          ASSERT (cond != NULL);
          ASSERT (lock != NULL);
          ASSERT (!intr_context ()); // interrupt context 내에서 호출되지 않았는지 확인
          ASSERT (lock_held_by_current_thread (lock)); 
        		// 현재 thread가 주어진 lock을 보유하고 있는지 확인
        
          if (!list_empty (&cond->waiters)) 
        		// cond의 대기자 List가 비어있지 않다면 (하나 이상의 thread가 대기 중이면)
            sema_up (&list_entry (list_pop_front (&cond->waiters),
                                  struct semaphore_elem, elem)->semaphore);
        		// 1. list_pop_front(&cond->waiters) : waiters list에서 첫번째 element 꺼내기
        		// 2. list_entry 함수를 사용하여 꺼낸 element를 struct sesmaphore_elem type으로 변환
        		// 3. 해당 semaphore_elem의 semaphore에 sema_up 연산을 수행하여 대기 중인 thread 하나를 깨우기
        }
        ```
        
        조건 변수 cond에 대기 중인 thread 중 하나에 신호를 보내어 해당 thread가 대기 상태에서 깨어나도록 해주는 함수다.
        
        → **`cond`**에 대기 중인 thread 중 하나를 깨우는 역할. 
        
        → 특정 조건이 만족될 때 대기 중인 thread를 깨워 해당 조건에 반응하도록 만드는 역할.
        
        - **void cond_broadcast (struct condition *, struct lock *);**
        
        ```c
        void
        cond_broadcast (struct condition *cond, struct lock *lock) 
        {
        	// cond : 깨울 대기 중인 thread들이 연결된 conditional variable
          // lock : conditional variable을 보호하는 lock
        
          // cond, lock이 NULL이 아닌지 확인
          ASSERT (cond != NULL);
          ASSERT (lock != NULL);
        
          while (!list_empty (&cond->waiters)) // waiters(대기 list)이 비어 있지 않은 동안 반복
          // 대기 중인 thread가 있다면 그 thread들을 깨우기 위한 작업을 계속 진행하는 것
            cond_signal (cond, lock); // cond에 대기 중인 thread 하나를 깨움
        } // -> 결국 하나씩 깨움으로써 cond에 대기 중인 모든 thread들이 깨어나게 됨
        ```
        
        cond에 대기 중인 모든 thread를 깨우는 함수이다.
        
        → 이는 여러 thread가 특정 조건을 기다리고 있고 그 조건이 만족됐을 때, 모든 thread들에게 동시에 알릴 필요가 있을 때 유용하게 사용된다.
        

---

# **2. Analysis of the current thread system**

### **A. Structure**

- **thread 관리에 필요한 변수들**
    - thread.h
        - States in a thread's life cycle
        
        ```c
        enum thread_status
          {
            THREAD_RUNNING,     /* Running thread. */
            THREAD_READY,       /* Not running but ready to run. */
            THREAD_BLOCKED,     /* Waiting for an event to trigger. */
            THREAD_DYING        /* About to be destroyed. */
          };
        ```
        
        thread의 status를 표현하는 4가지 상태에 대한 변수이다.
        
        | THREAD_RUNNING | CPU에서 현재 실행 중인 상태 | thread가 실행 중 |
        | --- | --- | --- |
        | THREAD_READY | 실행할 준비는 되었지만 실행되고 있지 않은 상태 | ready list에 있는 경우 |
        | THREAD_BLOCKED | lock, interrupt 등의 trigger을 기다리고 있는 상태 | thread_block()이나 Interrupt에 의해 block된 경우 → thread_unblock()함수에 의해 다시 READY로 가지 않으면 스케쥴링 되지 않음 |
        | THREAD_DYING | 다른 thread로 전환하고 사라질 상태 | scheduler에 의해 destroy된 경우 |
        
        - **Thread identifier type**
        
        `typedef int tid_t;`  Thread Identifier의 약자로, 각 thread의 식별 번호인 tid 값을 저장한다. 
        
        - **Thread priorities**
        
        priority는 0부터 63 사이의 정수값을 가진다. 숫자가 클수록 우선순위가 높으며, default 값은 31이다.
        
        ```c
        #define PRI_MIN 0                       /* Lowest priority. */
        #define PRI_DEFAULT 31                  /* Default priority. */
        #define PRI_MAX 63                      /* Highest priority. */
        ```
        
    - thread.c
        - `static struct list ready_list;` THREAD_READY state에 있는 process들의 리스트이다. 실행할 준비가 되었지만, 실제로는 **실행되고 있지 않은 상태**를 말한다.
        - `static struct list all_list;` 모든 process들의 리스트이다. 처음 schedule 될 때 리스트에 추가되고, exit될 때 삭제된다.
        - `static struct thread *idle_thread;` idle thread이고, ready list가 비었을 때 실행된다.
            - system에 다른 실행할 준비가 된 thread가 없을 때 실행되며 CPU가 할 일이 없을때 idle_thread가 실행되어 CPU의 시간을 소모한다.
            - idle_thread는 OS에 의해 가장 낮은 우선순위로 설정되며 다른 모든 thread가 실행되지 않을 때만 실행된다.
        - `static struct thread *initial_thread;` init.c의 main()에서 실행되고, initial thread이다.
        - `static struct lock tid_lock;` new thread의 tid를 allocate하기 위하여 lock을 사용한다.
            - 동일한 tid가 2번 이상 할당되는 것을 방지하기 위해 lock이 사용된다.
                1. thread가 새 tid를 할당받고자 할 때 allocate_tid() 함수를 호출한다.
                2. 함수 내에서 먼저 tid_lock을 획득한다.
                3. lock을 획득한 후 안전하게 새로운 tid를 할당 받는다.
                4. tid 할당 후 tid_lock을 release한다.
        
        - struct kernel_thread_frame : kernel_thread()을 위한 Stack frame이다.
        
        ```c
        struct kernel_thread_frame 
          {
            void *eip;                  /* Return address. */
            thread_func *function;      /* Function to call. */
            void *aux;                  /* Auxiliary data for function. */
          };
        ```
        
        - **Timer** **Ticks**
        
        `static long long idle_ticks;`   idle thread를 위한 # of timer ticks 저장
        `static long long kernel_ticks;`  kernel threads를 위한 # of timer ticks 저장
        `static long long user_ticks;`  user programs을 위한 # of timer ticks 저장 
        
        - timer ticks : system의 timer interrupt가 발생하는 주기로 일정 시간 간격으로 발생한다.
        - **Scheduling**
        
        - `#define TIME_SLICE 4`  각 thread에게 4 timer ticks을 부여하고 실행시킨다.
            - 각 thread에 할당된 timer tick 수가 4라는 것
            - 각 thread는 연속으로 4개의 timer tick동안 실행될 수 있다.
            - 이는 round-robin scheduling algorithm의 일부분으로 각 thread는 일정한 시간(4개의 timer tick)동안 CPU를 점유하고 그 시간이 지나면 다음 thread에게 CPU를 양보한다.
        - `static unsigned thread_ticks;`
            - thread가 마지막으로 CPU를 양보한 이후의 timer tick 수를 추적
            - 매 timer interrupt마다 thread_ticks는 증가하며 thread_ticks 값이 time_slice와 동일하거나 그보다 크게 되면 현재 thread는 CPU를 양보하고 다음 thread가 실행된다.
            - 결국 thread_ticks 변수는 현재 thread가 얼마나 오랜 시간 동안 실행되었는지 추적하는 데 사용되며, 이를 통해 thread가 time_slice보다 더 오래 실행되지 않도록 한다.
        - `bool thread_mlfqs;` # thread scheduler을 정하는 flag이다.
            - false 값일 때(default) round-robin scheduler를 사용한다.
                - 기본 scheduling 방식으로 각 thread는 일정 시간동안 실행되며 그 시간이 지나면 다른 thread로 전환된다. 이를 통해 모든 thread에게는 균등한 시간이 제공된다.
            - true 값일 때 multi-level feedback queue scheduler를 사용한다.
                - thread의 priority를 고려하여 thread끼리 전환된다.
            - kernel command-line option "-o mlfqs"에 의해 제어된다.
            

- **Struct thread**
    
    ```c
    struct thread
      {
        /* Owned by thread.c. */
        tid_t tid;                          // 각 thread 별로 고유한 tid
        enum thread_status status;          // thread state
        char name[16];                      // name
        uint8_t *stack;                     // Saved stack pointer
        int priority;                       // Priority
        struct list_elem allelem;           // List element for all threads list
    
        /* Shared between thread.c and synch.c. */
        struct list_elem elem;              // List element
    
    #ifdef USERPROG
        /* Owned by userprog/process.c. */
        uint32_t *pagedir;                  // Page directory
    #endif
    
        /* Owned by thread.c. */
        unsigned magic;                     // Detects stack overflow
      };
    ```
    
    thread별로 고유의 tid를 가져 구별된다. 또한 status에는 해당 thread의 현재 state를 저장한다. name은 디버깅 시에 사용한다. thread는 고유의 stack을 가지는데 이 statck pointer의 값을 stack에 저장한다. 이 값은 switch할 때 사용된다. allelem, elem은 list를 관리하기 위해 쓰인다. thread의  list_elem을 통해서 다른 thread와 연결 가능하다.
    
    - allelem: 모든 thread를 연결한 list의 element
    - elem: ready_list, semaphore에서 wait하고 있는 list의 element
    
    pintos에서는 이와 같이 list element를 이용하여 특정 element에 접근할 수 있다. 
    
    pagedir은 page directory의 주소를 저장하고, magic은 stack overflow를 찾기 위해 임의의 값으로 설정된다.
    
    - thread_current function이 저장된 값과 원래 설정된 값을 비교하여 같지 않으면 stack overflow가 발생하였다고 감지하고 assertion을 발생시키게 된다.
    
- **List.h의 list 관련 변수들**

```c
/* List element. */
struct list_elem 
  {
    struct list_elem *prev;     /* Previous list element. */
    struct list_elem *next;     /* Next list element. */
  };

struct list 
  {
    struct list_elem head;      /* List head. */
    struct list_elem tail;      /* List tail. */
  };

#define list_entry(LIST_ELEM, STRUCT, MEMBER)           \
        ((STRUCT *) ((uint8_t *) &(LIST_ELEM)->next     \
                     - offsetof (STRUCT, MEMBER.next)))
```

list_entry는 elem이 속하는 thread의 주소를 구하는 매크로이다. 
이 외에 traversal, insertion, removal, elements, properties 등을 위한 여러 가지 변수들이 정의되어 있다. 

### **B. Functions**

- **void thread_init (void);** → thread system의 초기 설정을 하기 위한 function

```c
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF); //intr_get_level()을 사용해
																					//현재 인터럽트 비활성화되어 있는지 확인
																					// -> 인터럽트가 비활성화된 상태에서만 실행하도록 보장함

  lock_init (&tid_lock); 
  list_init (&ready_list); 
  list_init (&all_list);

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid (); // tid를 할당시킴
}
```

현재 실행중인 code를 thread로 전환하여 thread system을 초기화하는 함수이다. run queue와 tid lock을 초기화시킨다. 이 함수를 호출한 이후에, thread를 만드는 thread_create()함수 이전에 page allocator을 초기화 해야한다. 또한, 이 thread_init() 함수가 종료되기 전까지는 thread_current()를 호출하는 것은 안전하지 않다. 

- **void thread_start (void);** → scheduler를 시작하기 위한 function

```c
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started; // thread 간의 동기화를 위해 사용된다.
  sema_init (&idle_started, 0); // semaphore를 0으로 초기화
  thread_create ("idle", PRI_MIN, idle, &idle_started); // idle 이름의 thread 생성

  /* Start preemptive thread scheduling. */
  intr_enable (); // 실행 중인 다른 thread를 중단시키고 cpu를 점유하도록 하는 preemptive scheduling을 시작

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started); //semaphore 값이 0이 되면 계속 진행해서 이후 코드 실행
}
```

scheduler를 시작하는 함수이다. interrupt를 enable하여 preemptive thread를 시작한다. 또한 idle thread를 생성한다. 

- **void thread_tick (void);** → 일정 시간이 지날 때마다 timer interrupt

```c
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread) // 현재 스레드가 idle 스레드인 경우
    idle_ticks++;
#ifdef USERPROG 
// 현재 스레드가 유저 프로세스 스레드인 경우
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
	// thread 실행된 틱 수를 증가시킴
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return (); // TIME_SLICE마다 preemption 수행시킨
}
```

각 timer tick마다 timer interrupt handler에 의해 호출된다. 이 함수는 external interrupt context에서 실행된다.
이때 TIME_SLICE 마다 preemption을 수행하여서 특정 시간 이상 실행된 thread를 yield 시킴으로써 균형을 맞춘다.

- **void thread_print_stats (void);** → thread의 statistic을 출력

```
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}
```

`typedef void thread_func (void *aux);`

- **tid_t thread_create (const char *name, int priority, thread_func *function, void *aux)**
    
    → 새로운 thread 생성
    

```
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO); // 페이지 할당
  if (t == NULL) // 실패한 경우 에러
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority); // parameter로 초기화시킴
  tid = t->tid = allocate_tid (); // tid 할당함

	// 함수 호출을 위한 stack frame을 설정

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  /* Add to run queue. */
  thread_unblock (t);

  return tid; // tid return함
}
```

새 kernel thread를 생성한다. name, priority를 parameter로 받아서 thread 변수를 초기화하고, aux를 function 함수의 인자로 전달한다. 또한 thread를 ready queue에 추가하여 대기 상태로 만든다, 생성 완료되면 tid를 return하고, 실패한 경우에는 TID_ERROR를 return한다. 

만약 이미 **`thread_start()`** 함수가 호출된 경우, 새로운 thread는 **`thread_create()`** 함수가 반환하기 전에도 스케줄링될 수 있다. 또한 새로운 thread는 **`thread_create()`** 함수가 반환하기 전에 이미 종료될 수도 있다. 

반대로, 원래 thread는 새로운 thread가 스케줄링되기 전에 어떤 시간 동안이든 실행될 수 있다. 

만약 순서를 보장해야하는 경우에는 semaphore 등의 synchronization의 도구를 사용해야 한다. 

→ 위의 주어진 코드는 새로운 thread의 우선 순위를 PRIORITY로 설정하지만, 실제로 priority scheduling은 구현되지 않았으므로, 본 lab에서 구현해야 한다. 

- **void thread_block (void);** → state를 running에서 blocked로 전환

```c
void
thread_block (void) 
{
  ASSERT (!intr_context ()); // 현재 interrupt context가 아닌지 확인
  ASSERT (intr_get_level () == INTR_OFF); // 현재 interrupt 비활성화되었는지 확인

  thread_current ()->status = THREAD_BLOCKED; // 현재 실행 중인 thread의 상태를 BLOCKED로 설정
  schedule (); // 새로운 thread로 context switching
```

현재 실행중인 thread의 state를 sleep 상태로 바꾼다. thread_unblock()함수를 이용해 깨우지 않는 이상 다시 schedule되지 않을 것이다. 또한 이 함수는 synchronization의 영향을 고려하여, interrupt가 꺼진 상태로 호출되어야만 한다. 

- **void thread_unblock (struct thread *);** → blocked된 thread를 다시 ready state로 전환

```c
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t)); // 유효한 thread인지 확인

  old_level = intr_disable (); // Interrupt 비활성화
  ASSERT (t->status == THREAD_BLOCKED); // thread 상태 확인
  list_push_back (&ready_list, &t->elem); // ready list에 추가
  t->status = THREAD_READY; // 상태 변경
  intr_set_level (old_level); // 이전 Interrupt level 복원
}
```

Interrupt 비활성화 시킨 후, block 되어 있는 thread를 ready list에 추가하고, ready 상태로 변경해 thread status를 업데이트한다. 

- **struct thread *thread_current (void); →** 현재 실행중인 thread 주소를 return한다.

```c
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t)); // thread인지 확인
  ASSERT (t->status == THREAD_RUNNING); // 현재 running 상태인지 확인

  return t;
}
```

- **tid_t thread_tid (void);** → 현재 실행중인 thread의 tid를 return한다.

```c
tid_t
thread_tid (void) 
{
  return thread_current ()->tid; 
}
```

- **const char *thread_name (void);** → 현재 실행중인 thread의 name을 return한다.

```
const char *
thread_name (void) 
{
  return thread_current ()->name;
}
```

- **void thread_exit (void) NO_RETURN;** → thread가 종료될 때 호출되고, return 존재하지 않는다.

```c
void
thread_exit (void) 
{
  ASSERT (!intr_context ()); // interrupt context 아닌지 확인

#ifdef USERPROG
  process_exit (); // 사용자 모드일 경우 process 종료
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable (); // intr 비활성화
  list_remove (&thread_current()->allelem); // 현재 thread 목록에서 현재 thread 제거한다
  thread_current ()->status = THREAD_DYING; // DYING 상태로 지정한다
  schedule (); // 다음 process로 context switch
  NOT_REACHED (); // 이 지점에 도달하면 오류가 난다, 도달하면 안된다
}
```

- **void thread_yield (void);** → thread switch 할 때 사용

```
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ()); // interrupt context 아닌지 확인

  old_level = intr_disable (); 
  if (cur != idle_thread) 
    list_push_back (&ready_list, &cur->elem); // ready list에 추가
  cur->status = THREAD_READY; // 현재 thread의 상태를 READY로 변경
  schedule (); 
  intr_set_level (old_level);
}
```

cpu를 yield하여 다른 thread가 cpu를 점유하고 실행될 수 있도록 한다. 현재 thread는 sleep되지 않고, 이후에 다시 schedule 될 수 있다.

- 주어진 thread t에 대해 auxiliary data aux를 이용하여 작업을 수행

`typedef void thread_action_func (struct thread *t, void *aux);`

- **void thread_foreach (thread_action_func *, void *);**

```
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF); // interrupt 상태 확인

  for (e = list_begin (&all_list); e != list_end (&all_list); //list 처음부터 끝까지 순회
       e = list_next (e)) // 각 요소 가져옴
    {
      struct thread *t = list_entry (e, struct thread, allelem); 
			// list_entry를 이용하여 thread 구조체로 변환
      func (t, aux); // func 실행
    }
}
```

all_list의 모든 thread를 순회하면서, parameter인 function을 수행한다. 이때 인자로 받은 aux를 parameter 값으로 이용한다. 

- **void thread_set_priority (int);** → 현재 thread의 priority를 new_priority로 바꾼다

```
void
thread_set_priority (int new_priority) 
{
  thread_current ()->priority = new_priority;
}
```

- **int thread_get_priority (void);** → 현재 thread의 priority를 return한다.

```c
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}
```

- **static void kernel_thread (thread_func *, void *aux);**

```c
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL); // kernel thread가 실행할 함수 pointer가 유효한지 확인

  intr_enable ();       /* The scheduler runs with interrupts off. */
	// interrupt 활성화 시키는 것
  // scheduler는 interrupt가 비활성화된 상태에서 실행되도록 설계되어있기 때문에
  // thread가 시작되면 interrupt를 다시 활성화하여 정상적인 동작을 하도록 함
  function (aux);       /* Execute the thread function. */
	// function pointer을 호출하며 이 때 aux를 인자로 전달
  // kernel thread가 시작할 때 실행할 함수와 그 함수에 필요한 보조 데이터를 전달하는 것
  thread_exit ();       /* If function() returns, kill the thread. */
  // function 함수 실행이 완료되면 현재 thread가 종료
}
```

kernel_thread 함수는 결국 주어진 function을 실행하는 kernel thread를 시작하고, 해당 함수가 실행 완료되면 thread를 종료한다.

- **static void idle (void *aux UNUSED);**

```c
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current (); // 현재 thread를 전역 변수인 idle_thread에 할당
  
	sema_up (idle_started); 
	// idle thread가 시작되었음을 다른 thread나 함수에 알리는 역할
  // pintos 시작부분에서 thread_start() 함수에서 idle thread가 준비될 때까지 기다리기 위해
	// semaphore가 사용된다
  // 이 semaphore는 0으로 초기화되어 thread_start()는 대기 상태에 머물게 되는데
  // idle thread가 시작되면 sema_up을 호출함으로써 semaphore 값을 증가시켜
  // thread_start()함수가 계속 진행되도록 한다.
  // 결국 idle thread가 완전히 시작되고 준비된 후에만 thread_start() 함수가 다음 작업을 진행하도록 함

  for (;;) // 무한 loop - system에 실행할 thread가 없을 때 계속 실행
    {
      /* Let someone else run. */
      intr_disable (); // interrupt 비활성화 
      thread_block (); // 현재 thread(idle thread)를 block해서 다른 thread가 실행될 수 있게 함
			// thread_block() 호출 전에 interrupt 발생시 문제가 발생할 수 있으므로
      asm volatile ("sti; hlt" : : : "memory");
			// sti : system의 interrupt을 활성화하는 역할
			// idle thread는 CPU가 놀고 있을 때만 실행되므로 
      // 다른 thread가 준비되면 해당 thread를 즉시 실행하기 위해 interrupt를 활성화해야 함
      // hlt : CPU를 sleep 상태로 만드는 것
			// CPU가 불필요하게 소비되는 것을 방지하기 위해 CPU를 멈추게 하며
      // interrupt 발생시에 CPU는 hlt 상태에서 깨어나 동작을 재개
    }
}
```

idle thread는 실제로 아무 작업도 수행하지 않기 때문에 CPU resource를 낭비하게 된다. 따라서 hlt 명령을 사용하여 CPU를 멈추게 하려고 한다. hlt 명령을 실행하기 전에 interrup를 활성화하여 다른 thread가 준비되거나 다른 interrupt가 발생할 때 CPU가 다시 깨어나게 한다.

- **static struct thread *running_thread (void);**

```c
/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp; // stack pointer을 저장하는 변수 선언

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  // CPU의 현재 stack pointer 값을 esp 변수에 저장
  return pg_round_down (esp);
  // 주어진 주소를 가장 가까운 page 시작 주소로 내림하여 반환
  // struct thread는 항상 page 시작 부분에 위치하므로
  // stack pointer을 page 시작 부분으로 내리면 현재 thread의 struct thread 주소를 얻을 수 있음
}
```

이 함수는 현재 실행 중인 thread의 stack pointer을 사용하여 해당 thread의 구조체 주소를 찾아 반환한다.

- **static struct thread *next_thread_to_run (void);**

```c
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list)) // ready_list(실행 가능하게 준비된 threads)이 비어있는지 확인
    return idle_thread; // empty면 다른 thread가 실행할 준비가 되지 않았을 때 실행되는 특수한 thread
  else // not empty case
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
		// 가장 앞에 있는(준비 상태의 Thread 중 가장 먼저 들어온) thread를 가져와서 실행하기 위한 준비
    // list_entry 함수를 통해 해당 thread 구조체의 주소를 가져옴
}
```

실행 가능한 thread가 있으면 그 중에서 하나를 선택하여 반환하며 실행 가능한 thread가 없으면 idle_thread를 반환

- **static void init_thread (struct thread *, const char *name, int priority);**

```c
/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  enum intr_level old_level;

  ASSERT (t != NULL); // thread가 null이 아닌지 
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX); 
	// 입력된 priority가 시스템에서 설정한 범위 안의 값인지 확인
  ASSERT (name != NULL); // 입력된 name이 null이 아닌지 확인

  memset (t, 0, sizeof *t); // thread 구조체를 0으로 초기화
  t->status = THREAD_BLOCKED; // thread 상태를 blocked로 설정
  strlcpy (t->name, name, sizeof t->name); // thread 이름 설정
  t->stack = (uint8_t *) t + PGSIZE; // thread stack 설정 (page size)
  t->priority = priority; // priority 설정
  t->magic = THREAD_MAGIC; // magic은 stack overflow를 감지하기 위해 필요한 값으로, 이후 struct에서 설명

  old_level = intr_disable (); // intrpt 비활성화하고 이전의 intrpt 레벨을 저장함
  list_push_back (&all_list, &t->allelem); // 모든 thread에 대한 all_list에 현재 thread의 elem 저장
  intr_set_level (old_level); // 이전 intrpt 레벨 복웒
}
```

thread t를 주어진 이름 name으로, blocked된 thread로 초기화하는 함수다.

- **static bool is_thread (struct thread *) UNUSED;**

```c
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}
```

thread pointer를 인자로 받아, 유효한 thread가 맞는지 확인한다. 이때 thread magic 값을 비교하여 stack overflow가 일어났는지 확인하여, 유효한지 확인하는 과정이 있다. (Stack overflow가 일어나면 해당 값을 수정하기 때문에 원래 값과 현재 값을 비교하여 확인 가능하다.)

- **static void *alloc_frame (struct thread *, size_t size);**

```c
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t)); // thread인지 확인
  ASSERT (size % sizeof (uint32_t) == 0); // size의 규칙을 만족하는지 확인한다

  t->stack -= size;// thread의 stack size를 할당하고자 하는 size만큼 뺄셈한다
  return t->stack; // stack 
}
```

여기서 thread→stach은 thread struct의 멤버변수로, stack pointer 값을 저장하는 uint8_t 형 변수이다. 

이때 `ASSERT (size % sizeof (uint32_t) == 0);`에서는 입력한 size가 uint32_t의 배수인지 검증한다. 즉, stack size가 word(32bit) 단위로 정렬되는지 확인하여, stack frame을 할당한다. 

- **static void schedule (void);**

```c
static void
schedule (void) 
{
  struct thread *cur = running_thread (); // 현재 실행중인 thread 저장
  struct thread *next = next_thread_to_run (); // 다음에 실행될 thread
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF); // 비활성 상태인지 확인
  ASSERT (cur->status != THREAD_RUNNING); // 현재 상태가 Running인지 아닌지
  ASSERT (is_thread (next)); // 다음 실행될 thread가 유효한지

  if (cur != next) // 현재 thread와 다음에 실행될 thread가 다른 경우에 switch한다
    prev = switch_threads (cur, next); // 전환하고, 이전 실행 thread를 return하여 prev에 저장한다
  thread_schedule_tail (prev); // thread의 상태 업데이트하고 마무리한다
}
```

다음에 실행할 thread를 확인하여 context switch시킨다. 이후 `thread_schedule_tail`함수를 이용하여 prev(실행이 중단된 thread)의 후처리 작업을 해준다. 

- **static tid_t allocate_tid (void);** → 새로운 thread가 사용할 tid를 return

```c
/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1; // 다음 tid를 1로 지정
  tid_t tid;

  lock_acquire (&tid_lock); // lock을 걸어 synchronization
  tid = next_tid++; // 다음 tid를 증가시킴
  lock_release (&tid_lock);

  return tid;
}
```

> 구현해야할 함수 (Not yet implemented)
> 
> - int thread_get_nice (void);
> - void thread_set_nice (int);
> - int thread_get_recent_cpu (void);
> - int thread_get_load_avg (void);

### **C. How to switch threads**

- **schedule** → thread를 전환하는 함수

```c
static void
schedule (void) 
{
  struct thread *cur = running_thread (); // 현재 실행중인 thread 저장
  struct thread *next = next_thread_to_run (); // 다음에 실행될 thread
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF); // 비활성 상태인지 확인
  ASSERT (cur->status != THREAD_RUNNING); // 현재 상태가 Running인지 아닌지
  ASSERT (is_thread (next)); // 다음 실행될 thread가 유효한지

  if (cur != next) // 현재 thread와 다음에 실행될 thread가 다른 경우에 switch한다
    prev = switch_threads (cur, next); // 전환하고, 이전 실행 thread를 return하여 prev에 저장한다
  thread_schedule_tail (prev); // thread의 상태 업데이트하고 마무리한다
}
```

- **void thread_schedule_tail (struct thread *prev)** → 새로 실행된 thread가 running 상태라고 표시

```
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  cur->status = THREAD_RUNNING; // 상태를 Running이라고 지정
  thread_ticks = 0; // 새로운 time slice 시작

#ifdef USERPROG // 유저 프로그램에서
  process_activate (); // 새로운 address space 활성화
#endif

  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
	// 이러한 조건을 만족할 경우에는, 해당 thread의 page를 free 시킨다
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}
```

간단히 말하자면, scheduling의 후처리 작업을 수행하는 함수이다.

switching이 일어나서 새로 실행되는 thread가 running중이라고 표시하고, 또한 switching이 일어나기 전 원래 thread가 dying state이면 해당 thread의 page를 free시켜서 후처리 작업을 해준다. 

- **static tid_t allocate_tid (void)** → 새로운 thread를 위한 tid를 return

```c
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1; // 1부터 시작
  tid_t tid;

  lock_acquire (&tid_lock); // sync를 위한 Lock acquir
  tid = next_tid++; // 다음 tid 갱신
  lock_release (&tid_lock); // 작업이 끝났으므로 해제

  return tid;
}
```

---

# **3. How to achieve each requirement**

- overall structure
- data structures, functions to be added / modified • algorithms, rationales, and so on.

## A. Alarm clock

### **1) Problem : Current Implementation**

현재의 **`time_sleep`**함수는 busy waiting 방식으로 구현되어 있다.

```
void
timer_sleep (int64_t ticks) 
{
  int64_t start = timer_ticks ();

  ASSERT (intr_get_level () == INTR_ON);
  while (timer_elapsed (start) < ticks) 
    thread_yield ();
}
```

interrupt가 on 상태일때 실행되고, 

### **2) Our Solution**

---

## B. Priority Scheduling

### **1) Problem : Current Implementation**

### **2) Our Solution**

---

## C. Advanced Scheduler

### **1) Problem : Current Implementation**

### **2) Our Solution**