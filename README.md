Run polling and epoll examples, then run the following:

```
wrk -t8 -c10000 -d60s --timeout 10s http://127.0.0.1:8080/
```
...to see a smaaaall diff.

```
wrk -t14 -c14000 -d60s --timeout 10s http://127.0.0.1:8080/
```
...to see a slightly larger diff.

Run to see num syscalls:
```
strace --follow-forks --summary build/io_uring
```
Do this with a smaller wrk run:
```
wrk -t1 -c1000 -d10s --timeout 10s http://127.0.0.1:8080/
```
