	
	//for load
	// for(int i=0;i<=globalIterStackIdx;i++) loadList[campID].i8[i] = globalIterStack.i8[i];
	// memcpy(loadList[campID].i8, globalIterStack.i8, STK_MAX_SIZE);
	// for(int i=0;i<STK_MAX_SIZE_DIV_BY_8;i++) loadList[campID].i64[i] = globalIterStack.i64[i];

	//for store
	// for(int i=0;i<=globalIterStackIdx;i++) storeHistory[addr].iterStack.i8[i] = globalIterStack.i8[i];
	// memcpy(storeHistory[addr].iterStack.i8, globalIterStack.i8, STK_MAX_SIZE);
	// for(int i=0;i<=STK_MAX_SIZE_DIV_BY_8;i++) storeHistory[addr].iterStack.i64[i] = globalIterStack.i64[i];









	// printf("=-=-=-=-= Dependence Table =-=-=-=-=\n");
	// //this is crazy std::string corrupt stack memory
	// std::string path;
	// path.resize(40);
	// path = "DependenceTable.data";

	// //#########################
	// //############file IO (ostream style)
	// std::ofstream *outfile = new std::ofstream;
	// outfile->open(path, std::ios::out | std::ofstream::app | std::ofstream::binary);
	// outfile->close();
	// delete outfile;
	// for(auto it : depTable){printf("C\n");}

	// // if(outfile.is_open()){
	// 	printf("A\n");
	//  	for (std::unordered_map<DepID, IterRelation>::iterator it=depTable.begin(), itE = depTable.end(); it!=itE; ++it) {
	//  		printf("b\n");
	// // 		CampID src = (CampID) (it->first >> 32);
	// // 		CampID dst = (CampID) (it->first & 0xffffffff);			
	// // // 		//human-readable form
	// // // 		CntxID srcCtx = (CntxID) (src>>16);
	// // // 		InstrID srcInst = (InstrID) (src & 0xffff);
	// // // 		CntxID dstCtx = (CntxID) (dst>>16);
	// // // 		InstrID dstInst = (InstrID) (dst & 0xffff);
	// // // 		std::cout<<srcCtx<<", "<<srcInst<<" => "<<dstCtx<<", "<<dstInst<<" : ";
	// // // 		std::cout<<std::bitset<64>(it->second)<<"\n";

	// // 		//compressed form
	// // 		outfile	<< src << ">" << dst << ":"
	// // 				<< it->second << "\n";
	//  	}
	// // }
	// // outfile.close();





	// printf("=-=-=-=-= Dependence Table =-=-=-=-= %d\n");
	// for (std::unordered_map<DepID, IterRelation>::iterator it=depTable.begin();
	// 		it!=depTable.end(); ++it) {
	// 	CampID src = (CampID) (it->first >> 32);
	// 	CampID dst = (CampID) (it->first & 0xffffffff);
	// 	CntxID srcCtx = (CntxID) (src>>16);
	// 	InstrID srcInst = (InstrID) (src & 0xffff);
	// 	CntxID dstCtx = (CntxID) (dst>>16);
	// 	InstrID dstInst = (InstrID) (dst & 0xffff);
	// 	//human-readable form
	// 	// std::cout<<srcCtx<<", "<<srcInst<<" => "<<dstCtx<<", "<<dstInst<<" : ";
	// 	// std::cout<<std::bitset<64>(it->second)<<"\n";

	// 	//compressed form
	// 	std::cout<< std::hex << src << ">" << dst << ":";
	// 	std::cout<< std::hex << it->second << "\n";
	// }