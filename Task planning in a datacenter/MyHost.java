/* Implement this class. */

import java.util.Comparator;
import java.util.LinkedList;
import java.util.Queue;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicInteger;

public class MyHost extends Host {

    //vom folosi un PriorityBlockingQueue pentru o sincronizare mai usoara si pentru a sorta task-urile
    //in functie de prioritate, iar in caz de egalitate in functie de ID
    private BlockingQueue<Task> taskQueue = new PriorityBlockingQueue<>(10, new Comparator<Task>() {
        @Override
        public int compare(Task o1, Task o2) {
            if (o2.getPriority() != o1.getPriority())
                return o2.getPriority() - o1.getPriority();
            return o1.getId() - o2.getId();
        }
    });
    private Task task = null;

    @Override
    public void run() {
        //cat timp nu s-a dat shutdown
        while(ok == 0){
            synchronized (this){
                //Ii dam lui task o valoare noua
                if(task == null || task.getFinish() != 0){
                    task = taskQueue.poll();
                }
            }
            //daca aceasta valoare exista
            if(task != null){
                //il putem sa proceseze o secunda
                try {
                    Thread.sleep(1000);
                } catch (InterruptedException e) {
                    throw new RuntimeException(e);
                }
                //ii spunem task-ului ca am executat o secunda
                task.setLeft(task.getLeft() - 1000);
                //daca am terminat de prelucrat task-ul, atunci il schimbam
                if(task.getLeft() <= 0){
                    task.finish();
                    task = null;
                }
                //daca exista un task cu o prioritate mai mare in coada iar task-ul curent este preemtabil, atunci le interschimbam
                if(task != null && task.isPreemptible() == true && task.getFinish() == 0 && taskQueue.peek() != null && taskQueue.peek().getPriority() > task.getPriority()){
                    Task aux = null;
                    aux = taskQueue.poll();
                    taskQueue.add(task);
                    task = aux;
                }
            }
        }
    }

    @Override
    public void addTask(Task task) {
        //adaugam task-ul primit in coada
        taskQueue.add(task);
    }

    @Override
    public int getQueueSize() {
        //daca avem un task in executie, il adaugam la dimensiunea cozii, in caz contrar
        //pastram doar dimensiunea cozii
        if(task == null)
            return taskQueue.size();
        return taskQueue.size() + 1;
    }

    @Override
    public long getWorkLeft() {
        long ret = 0;
        //daca nu exista nici dimensiune nici task in executie atunci returnam 0
        if(taskQueue.size() == 0 && task == null){
            ret = 0;
        }
        //daca dimensiunea cozii este 0 atunci vom returna secundele ramase de la task-ul curent
        else if(taskQueue.size() == 0){
            ret = task.getLeft();
        }
        //daca doar task-ul curent este nul atunci calculam suma secundelor ramase de lucru pentru fiecare task
        else if(task == null) {
            for (Task aux : taskQueue)
                ret += aux.getLeft();
        }
        //in caz contrat facem suma dintre task si secundele ramase ale task-urilor din coada
        else{
            for (Task aux : taskQueue)
                ret += aux.getLeft();
            ret +=  task.getLeft();
        }
        return ret;
    }

    int ok = 0;
    @Override
    public void shutdown() {
        ok = 1;
    }
}
