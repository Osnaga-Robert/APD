Pentru clasa MyDispacher am construit metoda addTask astfel:
    pentru fiecare tip specificat din cerinta am luat cate un caz separat(un switch despartit in 4 cazuri)
    cazul ROUND_ROBIN: in care mi-am luat un AtomicInteger pentru a fi mai usor de sincronicat si am uniformizat
        task-urile primite fiecarui host pe care il avem disponibil
    cazul SHORTEST_QUEUE: cazul in care am verificat care host are cea mai mica coada, iar in caz de egalite luam
        host-ul cu id-ul mai mic
    cazul SIZE_INTERVAL_TASK_ASSIGNMENT:cazul in care verificam tipul task-ului primit si infunctie de acesta
        trimitem pe ramura specifica(SHORT, MEDIUM sau LONG)
     cazul LEAST_WORK_LEFT: caz aproximativ la fel ca la SHORTEST_QUEUE dar calculam munca fiecarui host si il
        alegem pe cel care are mai putin timp de executat
Pentru clasa MyHost m-am folosit de doua obiecte private:
    PriorityBlockingQueue pentru a pastra lista de task-uri pentru fiecare host(am folosit BlockingQueue pentru
        a usura sincronizarea)
    Task pentru a pastra task-ul curent pe care il executa thread-ul
In metoda run() am facut logica pentru rularea task-urilor, am facut un loop pana cand este apelata metoda shutdown()
    avem cazul in care nu exista niciun task in executie sau daca task-ul prezent s-a terminat, caz in care incercam
        sa luam din coada un task
    avem cazul in care avem un task in executie, caz in care simulam ca task-ul sta o secunda pe procesor
        dupa care decrementam executia ramasa si verificam daca task-ul mai trebuie sa stea pe procesor
        (in cazul in care nu mai trebuie sa stea pe procsor, el va fi inlocui, ii vom da task-ului curent null pentru a-l
        schimba), de asemenea daca task-ul curent este preemtabil si exista un task cu o prioritate mai mare in coada atunci
        vom face o interschimbare
In metoda addTask() doar am adaugat task-ul curent in coada
In metoda getQueueSize() returnam dimensiunea cozii
In metoda getWorkLeft() am returnam munca ramasa fiecarei cozi, separat in 4 cazuri(cazul in care atat task-ul curent
    cat si coada sunt nule, cazul in care doar coada e goala, cazul in care doar task-ul curent e null
    si cazul in care niciuna nu este nula)
    