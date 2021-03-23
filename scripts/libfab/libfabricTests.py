import openpyxl
from pathlib import Path
import os
import subprocess
import re
from datetime import datetime

def read_excel():
	xlsx_file = Path('/root', 'libfabric.xlsx')
	wb_obj = openpyxl.load_workbook(xlsx_file)
	sheet = wb_obj.active

	col_names = []
	test_expected = []
	test_commands = []
	for column in sheet.iter_cols(1, sheet.max_column):
		col_names.append(column[0].value)
		test_commands.append(column[1].value)
		test_expected.append(column[2].value)

	test_dict = {}

	index = 0
	for column in col_names:
		if index > 0:
			test_dict[col_names[index]] = {"testName":col_names[index], "command":test_commands[index], "expected":test_expected[index]}
		index = index + 1

	return test_dict


##LOADING EXCEL DATA
test_dict = read_excel()
#print(test_dict)

##Log file defination
log_name = datetime.now().strftime('libfabric_%H_%M_%d_%m_%Y.log')
fylPntr = open("/tmp/{}".format(log_name), "a")
	

###TESTS

#TEST-1
output = os.popen(test_dict['Test-001: Verify whether libfabric present']['command'])
output = output.read()
#output = output.decode('utf-8')

print("===========================================================")
print("Test-001: Verify whether libfabric present")

if (test_dict['Test-001: Verify whether libfabric present']['expected']==output.strip()):
    print("PASS")
    fylPntr.write("PASS | Test-001: Verify whether libfabric present\n")
    fylPntr.write("===============================================\n")
    fylPntr.write("\n{}".format(test_dict['Test-001: Verify whether libfabric present']['command']))
    fylPntr.write("\n------------------------------------------------\n")  
    fylPntr.write("\n{}".format(output))
    fylPntr.write("\n------------------------------------------------\n")

else:
   print("FAIL")
   fylPntr.write("FAIL | Test-001: Verify whether libfabric present\n")
   fylPntr.write("===============================================\n")
   fylPntr.write("\n{}".format(test_dict['Test-001: Verify whether libfabric present']['command']))
   fylPntr.write("\n------------------------------------------------\n")
   fylPntr.write("\n{}".format(output))
   fylPntr.write("\n------------------------------------------------\n")

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#TEST-2
output = os.popen(test_dict['Test-002: Verify fi info provider tcp']['command'])
output = output.read()

print("===========================================================")
print("Test-002: Verify fi info provider tcp")

if re.search(test_dict['Test-002: Verify fi info provider tcp']['expected'], output.strip()):
    print("PASS")
    fylPntr.write("PASS | Test-002: Verify fi info provider tcp\n")
    fylPntr.write("===============================================\n")
    fylPntr.write("\n{}".format(test_dict['Test-002: Verify fi info provider tcp']['command']))
    fylPntr.write("\n------------------------------------------------\n")  
    fylPntr.write("\n{}".format(output))
    fylPntr.write("\n------------------------------------------------\n")
else:
    print("FAIL")
    fylPntr.write("FAIL | Test-002: Verify fi info provider tcp\n")
    fylPntr.write("===============================================\n")
    fylPntr.write("\n{}".format(test_dict['Test-002: Verify fi info provider tcp']['command']))
    fylPntr.write("\n------------------------------------------------\n")  
    fylPntr.write("\n{}".format(output))
    fylPntr.write("\n------------------------------------------------\n")
   
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#TEST-3
output = os.popen(test_dict['Test-003: Verify fi info provider sockets']['command'])
output = output.read()

print("===========================================================")
print("Test-003: Verify fi info provider sockets")

if re.search(test_dict['Test-003: Verify fi info provider sockets']['expected'], output.strip()):
    print("PASS")
    fylPntr.write("PASS | Test-003: Verify fi info provider sockets\n")
    fylPntr.write("===============================================\n")
    fylPntr.write("\n{}".format(test_dict['Test-003: Verify fi info provider sockets']['command']))
    fylPntr.write("\n------------------------------------------------\n")  
    fylPntr.write("\n{}".format(output))
    fylPntr.write("\n------------------------------------------------\n")
else:
    print("FAIL")
    fylPntr.write("FAIL | Test-003: Verify fi info provider sockets\n")
    fylPntr.write("===============================================\n")
    fylPntr.write("\n{}".format(test_dict['Test-003: Verify fi info provider sockets']['command']))
    fylPntr.write("\n------------------------------------------------\n")  
    fylPntr.write("\n{}".format(output))
    fylPntr.write("\n------------------------------------------------\n")
   
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#TEST-4
print("===========================================================")
print("Test-004: Verify fi info provider verbs")
output = os.popen(test_dict['Test-004: Verify fi info provider verbs']['command'])
output = output.read()

hostname = os.popen("hostname")
hostname = hostname.read()

if re.search("vm", hostname.strip()):
    if ((test_dict['Test-004: Verify fi info provider verbs']['expected']).strip()==output.strip()):
        print("PASS")
        fylPntr.write("PASS | Test-004: Verify fi info provider verbs\n")
        fylPntr.write("===============================================\n")
        fylPntr.write("\n{}".format(test_dict['Test-004: Verify fi info provider verbs']['command']))
        fylPntr.write("\n------------------------------------------------\n")  
        fylPntr.write("\n{}".format(output))
        fylPntr.write("\n------------------------------------------------\n")
    else:
        print("PASS | VM doesn't have 'verbs' as already known")
        fylPntr.write("PASS | VM doesn't have 'verbs' as already known | Test-004: Verify fi info provider verbs\n")
        fylPntr.write("===============================================\n")
        fylPntr.write("\n{}".format(test_dict['Test-004: Verify fi info provider verbs']['command']))
        fylPntr.write("\n------------------------------------------------\n")  
        fylPntr.write("\n{}".format(output))
        fylPntr.write("\n------------------------------------------------\n")
else:
    if re.search(test_dict['Test-004: Verify fi info provider verbs']['expected'], output.strip()):
        print("PASS | HW does have 'verbs'")
        fylPntr.write("PASS | Test-004: Verify fi info provider verbs\n")
        fylPntr.write("===============================================\n")
        fylPntr.write("\n{}".format(test_dict['Test-004: Verify fi info provider verbs']['command']))
        fylPntr.write("\n------------------------------------------------\n")  
        fylPntr.write("\n{}".format(output))
        fylPntr.write("\n------------------------------------------------\n")
    else:
        print("FAIL | HW doesn't have 'verbs'")
        fylPntr.write("FAIL | Test-004: Verify fi info provider verbs\n")
        fylPntr.write("===============================================\n")
        fylPntr.write("\n{}".format(test_dict['Test-004: Verify fi info provider verbs']['command']))
        fylPntr.write("\n------------------------------------------------\n")  
        fylPntr.write("\n{}".format(output))
        fylPntr.write("\n------------------------------------------------\n")
   
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#TEST-5
print("===========================================================")
print("Test-005: Verify fi ping pong")
start_server = subprocess.Popen('fi_pingpong -e msg -p tcp', shell=True)
output = os.popen(test_dict['Test-005: Verify fi ping pong']['command'])
output = output.read()	
subprocess.Popen.kill(start_server)

if re.search(test_dict['Test-005: Verify fi ping pong']['expected'], output.strip()):
    print("PASS")
    fylPntr.write("PASS | Test-005: Verify fi ping pong\n")
    fylPntr.write("===============================================\n")
    fylPntr.write("\n{}".format(test_dict['Test-005: Verify fi ping pong']['command']))
    fylPntr.write("\n------------------------------------------------\n")  
    fylPntr.write("\n{}".format(output))
    fylPntr.write("\n------------------------------------------------\n")
else:
    print("FAIL")
    fylPntr.write("FAIL | Test-005: Verify fi ping pong\n")
    fylPntr.write("===============================================\n")
    fylPntr.write("\n{}".format(test_dict['Test-005: Verify fi ping pong']['command']))
    fylPntr.write("\n------------------------------------------------\n")  
    fylPntr.write("\n{}".format(output))
    fylPntr.write("\n------------------------------------------------\n")
