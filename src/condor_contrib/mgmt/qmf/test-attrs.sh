#!/bin/sh

# cluster.proc from submit
echo "step #1 - SubmitJob"
job_id=$(./submit.py -q amqp://cumin/cumin@localhost:5672)
if [ $? -ne 0 ] ; then
echo "acl submit test - step #1 failed"
exit 1
fi

echo "step #2 - set good attribute name"
./setattr.py $job_id TEST "Value_" amqp://cumin/cumin@localhost:5672 > ./test-attr.2.out 2>&1
if [ $? -ne 0 ] ; then
echo "attr test - step #2 failed"
exit 1
fi

echo "step #3 - set really bad attribute name"
./setattr.py $job_id TEST "##@hds" amqp://evil/evil@localhost:5672 > ./test-attr.3.out 2>&1
if [ $? -eq 0 ] ; then
echo "attr test - step #3 failed"
exit 1
fi

echo "step #4 - set slightly bad attribute name"
./setattr.py $job_id TEST "Value_." amqp://evil/evil@localhost:5672 > ./test-attr.3.out 2>&1
if [ $? -eq 0 ] ; then
echo "attr test - step #3 failed"
exit 1
fi

echo "all tests passed"
