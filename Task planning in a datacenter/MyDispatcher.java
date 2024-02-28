/* Implement this class. */

import java.util.Comparator;
import java.util.List;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.PriorityBlockingQueue;
import java.util.concurrent.atomic.AtomicInteger;

public class MyDispatcher extends Dispatcher {

    public MyDispatcher(SchedulingAlgorithm algorithm, List<Host> hosts) {
        super(algorithm, hosts);
    }
    private AtomicInteger counter = new AtomicInteger(0);

    @Override
    public void addTask(Task task) {
        //cazuri pentru tipul algoritmului
        switch (algorithm){
            case ROUND_ROBIN:{
                //adaugam fiecare task uniform pentru toti hostii
                hosts.get(counter.getAndIncrement() % hosts.size()).addTask(task);
                break;
            }
            case SHORTEST_QUEUE:{
                AtomicInteger index = new AtomicInteger(0);
                AtomicInteger min = new AtomicInteger(100);
                synchronized (this){
                    for(Host host: hosts){
                        //in cazul in care gasim o coada mai mica, o inlocuim
                        if(host.getQueueSize() < min.get()){
                            index.set(hosts.indexOf(host));
                            min.set(host.getQueueSize());
                        }
                    }
                }
                //adaugam task-ul host-ului cu coada mai mica
                hosts.get(index.get()).addTask(task);
                break;
            }
            case SIZE_INTERVAL_TASK_ASSIGNMENT:{
                synchronized (this){
                    //luam 3 cazuri, ele reprezentand atat ordinea host-ului cat si tipul de task
                    switch(task.getType()){
                        case SHORT :{
                            //host-ul 0 este pentru task-urile scurte
                            hosts.get(0).addTask(task);
                            break;
                        }
                        case MEDIUM:{
                            //host-ul 1 este pentru task-urile medii
                            hosts.get(1).addTask(task);
                            break;
                        }
                        case LONG:{
                            //host-ul 2 este pentru task-urile lungi
                            hosts.get(2).addTask(task);
                            break;
                        }
                        default:{
                            break;
                        }
                    }
                }
                break;
            }
            case LEAST_WORK_LEFT:{
                int index = 0;
                float min = -1;
                    for(Host host: hosts){
                        //in cazul in care gasim un host care are de lucrat mai putin
                        //atunci pe acela il alegem
                        if(host.getWorkLeft() < min || min == -1){
                            index = (hosts.indexOf(host));
                            min =  host.getWorkLeft();
                        }
                    }
                hosts.get(index).addTask(task);
                break;
            }
            default:{
                break;
            }
        }
    }
}
