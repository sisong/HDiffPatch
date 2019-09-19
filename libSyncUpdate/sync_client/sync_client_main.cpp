//  sync_client_main.cpp
//  sync_client
//
//  Created by housisong on 2019-09-18.
//  Copyright © 2019 sisong. All rights reserved.
#include "sync_client.h"

int main(int argc, const char * argv[]) {
    return sync_patch_by_file(argv[1],argv[2],argv[3],0);
}
