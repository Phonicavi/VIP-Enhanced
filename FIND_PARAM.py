import os
import threading 
DIC = {};
DROP_LIST = [];
TASK = [];
cnt = 0;
THREADNUM = 4;

threads = []



def thd(srt,cnt):
	# global cnt
	while srt<cnt:
		print TASK[srt];
		os.system(TASK[srt]);
		
		srt += THREADNUM;

def main():
	# cnt = 0;
	global cnt
	
	for shrink_ratio in [5,6,7,8]:
		for req in range(20,70,10):
			cnt += 1;
			# TASK.append("VIP_FIND_params "+str(shrink_ratio/10.0) + " " + str(ratio_z/10.0) + " " + str(cnt) + ".tmp");
			TASK.append("./VIP_FIND  "+str(shrink_ratio/10.0) + " 0 " + str(req)+ " " +str(shrink_ratio) + "_" + str(req)  + ".tmp");

	for srt in range(THREADNUM):
		t1 = threading.Thread(target=thd,args=(srt,cnt))
		threads.append(t1)

	for t in threads:
		t.start()

	for t in threads:
		t.join()





	# for i in range(1,cnt+1):
	# 	try:
	# 		f = open(str(i)+".tmp","r");
	# 		con = f.read();
	# 		l = con.split();
	# 		DIC[(l[0],l[1])] = (float(l[2]),float(l[3]));
	# 		f.close();
	# 	except:
	# 		DROP_LIST.append(i);
	# # print DIC

 
	# ret = open("DIC","w");	
	# ret.write(str(DIC));
	# ret.close();

	# ret = open("DROP_LIST","w");
	# ret.write(str(DROP_LIST));
	# ret.close(); 

	# DICT = sorted(DIC.iteritems(), key=lambda d:d[1][1], reverse = False);
	# # print DICT
	# ret = open("FIND_ret","w");
	# for item in DICT:
	# 	ret.write(str(item)+"\n");
	# ret.close(); 


	# # ret = open("DROP_LIST","w");
	# # ret.write(str(DROP_LIST));
	# # ret.close(); 

	# os.system("del *.tmp");



if __name__ == '__main__':
	main()