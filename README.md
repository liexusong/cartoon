cartoon
-------
Save call stack when PHP process exception exit!

```php
example:

<?php
function test() {
    cartoon_coredump_debug(); /* Here would be make core dump */
}

test();
```

Result:
![Result](https://raw.githubusercontent.com/liexusong/cartoon/master/images/backtrace.png)