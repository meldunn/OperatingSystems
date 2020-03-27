
#include "sfs_api.h"
#include "disk_emu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>

//TODO: indirect ptr (for when need more than 12 blocks), not fresh, TRY TO USE VALGRIND TO FIND MEMORY LEAKS

directory_table dir_table;
free_inodes_bitmap free_inode_table;

fd_table file_desc_table;
superblock sfs_superblock;

inode inodes[MAX_NUMBER_OF_FILES];
file_descriptor root_directory;
file_entry dir[MAX_NUMBER_OF_FILES];

free_block_bitmap free_block;
inode_cache cache_inode;

//------------------------------------helper functions---------------------------------------------
//initialize dir table
void initialize_dir_table()
{
    for (int i = 0; i < MAX_NUMBER_OF_FILES; i++)
    {
        dir_table.directory[i].inode_number = -1; //set to no inode
                                                  // strncpy(dir_table.directory[i].filename, '\0', MAXFILENAME);
                                                  // dir_table.directory[i].filename = '\0';
    }
}

//initialize fd table
void initialize_fd_table()
{
    for (int i = 0; i < MAX_NUMBER_OF_FILES; i++)
    {
        file_desc_table.fd[i].i_node = -1; //set to null
        file_desc_table.next_avail = 0;    //index 0 is the next available
    }
}

//initialize free_inodes_bitmap
void initialize_free_inodes_bitmap()
{
    for (int i = 0; i < MAX_NUMBER_OF_FILES; i++)
    {
        free_inode_table.avail[i] = 1;
    }
}
void initialize_inodes()
{
    for (int i = 0; i < MAX_NUMBER_OF_FILES; i++)
    {
        inodes[i].gid = -1;
        inodes[i].link_cnt = -1;
        inodes[i].mode = -1;
        inodes[i].size = 0;
        inodes[i].ind_pointer = -1;
        inodes[i].uid = -1;
    }
    for (int i = 0; i < 12; i++)
    {

        inodes[i].block_pointer[i] = -1;
    }
}

//remove inode
void delete_inode(int index)
{
    for (int i = 0; i < 12; i++)
    {
        if (inodes[index].block_pointer[i] != -1)
        {
            // printf("/n/n block value in delete inode is %d\n", inodes[index].block_pointer[i]);
            free_block.map[inodes[index].block_pointer[i]] = 1; //set to free
            inodes[index].block_pointer[i] = -1;
            write_blocks(1023, 1, &free_block);
        }
    }
    inodes[index].gid = -1;
    inodes[index].link_cnt = -1;
    inodes[index].mode = -1;
    inodes[index].size = 0;
    inodes[index].ind_pointer = -1;
    inodes[index].uid = -1;
    //indicate that the inode is free in the inode bitmap
    free_inode_table.avail[index] = 1;
}

//initialize and write root directory
void initialize_root_directory()
{
    // for (int i =0; i < MAX_NUMBER_OF_FILES; i++){
    //     root_directory[i].inode_number = -1;
    //     strcpy(root_directory[i].filename, "\0"); //set to empty string
    // }
    root_directory.i_node = 0;
    free_inode_table.avail[0] = 0; //set as unavail
    root_directory.read_ptr = 0;
    root_directory.write_ptr = 0;
    file_entry root_file;
    root_file.inode_number = 0;
    // strncpy(root_file.filename, ".", MAXFILENAME);
    root_file.filename[0] = '.';
    dir[0] = root_file;
}

//initialize free block bitmap
void initialize_free_block_bitmap()
{
    for (int i = 0; i < NUMBER_OF_DATA_BLOCKS; i++)
    {
        if (i == 0 || (i >= 1 && i <= 25))
        {
            free_block.map[i] = 0; //set superblock, inode blocks as taken
        }
        else
        {
            free_block.map[i] = 1; //mark all blocks as free
        }
    }
}

//initialize inode cache
void initialize_inode_cache()
{
}

//get first free inode
int get_first_free_inode()
{
    for (int i = 0; i < MAX_NUMBER_OF_FILES; i++)
    {
        if (free_inode_table.avail[i] == 1)
        {
            //set as taken in bitmap
            free_inode_table.avail[i] = 0;
            return i;
        }
    }
    return -1; //no more free inodes left
}

//get first free block
int get_first_free_block()
{
    for (int i = 0; i < NUMBER_OF_BLOCKS; i++)
    {
        if (free_block.map[i] == 1)
        {
            //set as taken
            free_block.map[i] = 0;
            write_blocks(1023, 1, &free_block);
            return i;
        }
    }
    return -1; // no more free blocks
}

int get_num_free_blocks_remaining()
{
    int num_remaining = 0;
    for (int i = 0; i < NUMBER_OF_BLOCKS; i++)
    {
        if (free_block.map[i] == 1)
        {
            num_remaining++;
        }
    }
    return num_remaining;
}

// int get_position_of_block(int block_num)
// {
//     return (block_num * 1024);
// }

int get_next_avail_fd()
{
    int fd_to_give = file_desc_table.next_avail;
    //update the next avail fd
    for (int i = 0; i < MAX_NUMBER_OF_FILES; i++)
    {
        if (file_desc_table.fd[i].i_node == -1 && (i != file_desc_table.next_avail))
        {
            file_desc_table.next_avail = i;
            return fd_to_give;
        }
    }
    return -1;
}

void inode_set(int index, int first_free_block, int gid, int ind_pointer, int link_cnt, int mode, int size, int uid)
{
    inodes[index].block_pointer[0] = first_free_block;
    inodes[index].gid = gid;
    inodes[index].ind_pointer = ind_pointer;
    inodes[index].link_cnt = link_cnt;
    inodes[index].mode = mode;
    inodes[index].size = size;
    inodes[index].uid = uid;

    //update free inode bitmap incase its used for the first time
    free_inode_table.avail[index] = 0;
    write_blocks(1022, 1, &free_inode_table);
    write_blocks(1, 15, &inodes);
}

int get_first_free_dir_index()
{
    for (int i = 0; i < MAX_NUMBER_OF_FILES; i++)
    {
        if (dir_table.directory[i].inode_number == -1)
        {
            return i;
        }
    }
    return -1;
}

int get_inode_from_fileID(int fileID)
{
    return file_desc_table.fd[fileID].i_node;
}

int get_write_pointer_from_fileID(int fileID)
{
    return file_desc_table.fd[fileID].write_ptr;
}

int get_num_blocks_of_file(int inode)
{
    int num_owned = 0;
    for (int i = 0; i < 12; i++)
    {
        if (inodes[inode].block_pointer[i] != -1)
        {
            num_owned++;
        }
    }
    //check on indirect pointer
    if (inodes[inode].ind_pointer != -1)
    {
       // printf("ind pointer is: %d\n", inodes[inode].ind_pointer);
        indirect_block_pointer indirect_blocks;
        read_blocks(inodes[inode].ind_pointer, 1, &indirect_blocks);
        for (int i = 0; i < 128; i++)
        {
            int curr_block_num = indirect_blocks.ind_block[i];

            if (curr_block_num != -1)
            {
                num_owned++;
            }
        }
    }
    return num_owned;
}

//-------------------------------------create the file system---------------------------------------
void mksfs(int fresh)
{
    if (fresh)
    {
        if (init_fresh_disk("Melissa_Dunn_sfs", BLOCK_SIZE, NUMBER_OF_BLOCKS) != 0)
        {
            perror("error with initializing fresh disk");
        }

        //initialize inodes list
        //initialize_inodes();
        for (int i = 0; i < MAX_NUMBER_OF_FILES; i++)
        {
            delete_inode(i);
        }

        //initialize dir table
        initialize_dir_table();

        //initialize fd table
        initialize_fd_table();

        //initialize root
        initialize_root_directory();

        //initialize free inode bitmap
        initialize_free_inodes_bitmap();

        //initialize superblock
        sfs_superblock.magic = MAGIC;
        sfs_superblock.file_system_size = 1024;
        sfs_superblock.block_size = BLOCK_SIZE;
        sfs_superblock.inode_table_length = NUMBER_OF_INODE_BLOCKS;
        sfs_superblock.root_directory_inode = 0;

        //initialize free block bitmap
        initialize_free_block_bitmap();

        //write to disk
        //write super block
        write_blocks(0, 1, &sfs_superblock);

        //get amount of inode blocks and write inodes list
        int numiblocks = (sizeof(inodes) / BLOCK_SIZE);
        if (sizeof(inodes) % BLOCK_SIZE != 0)
        {
            numiblocks += 1;
        }
        //printf("num inode blocks: %d", numiblocks);
        write_blocks(1, numiblocks, &inodes);

        //         //write root directory
        // ``      int num_root_dir_blocks = (sizeof()

        //update free block bitmap
        free_block.map[0] = 0;    //superblock
        free_block.map[1022] = 0; //free inode bitmap block
        free_block.map[1023] = 0; //free block bitmap
        for (int i = 0; i < numiblocks; i++)
        {
            free_block.map[i + 1] = 0; //inode block starts at index 1
        }

        //write free inode and free block bitmaps
        write_blocks(1022, 1, &free_inode_table);
        write_blocks(1023, 1, &free_block);
    }
    else
    {
    }
}

//---------------------------------------sfs_getnextfilename----------------------------------------------

int sfs_getnextfilename(char *fname) //get the name of the next file in directory
{
    for (int i = dir_table.current_index; i < MAX_NUMBER_OF_FILES; i++)
    {
        if (dir_table.directory[i].inode_number != -1)
        {
            strncpy(fname, dir_table.directory[i].filename, MAXFILENAME);
            dir_table.current_index = i + 1;
            return 1;
        }
    }
    dir_table.current_index = 0;
    return 0;
}
//--------------------------------------sfs_getfilesize---------------------------------------------------

int sfs_getfilesize(const char *path) //get the size of the given file
{
    for (int i = 0; i < MAX_NUMBER_OF_FILES; i++)
    {
        if (strcmp(dir_table.directory[i].filename, path) == 0)
        {
            int path_inode = dir_table.directory[i].inode_number;
            return inodes[path_inode].size;
        }
    }
    return -1; //file doesnt exist
}

//--------------------------------------sfs_open---------------------------------------------------------

int sfs_fopen(char *name) //opens the given file
{
    //check if file size is ok
    if (strlen(name) > MAXFILENAME)
    {
        perror("file name too big");
        return -1;
    }

    //check if file exists already
    for (int i = 0; i < MAX_NUMBER_OF_FILES; i++)
    {
        if (strcmp(dir_table.directory[i].filename, name) == 0)
        {
            //file exists
            int file_inode;
            int file_fd;

            //get inode of the file
            for (int j = 0; j < MAX_NUMBER_OF_FILES; j++)
            {
                if (strcmp(dir_table.directory[j].filename, name) == 0)
                {
                    file_inode = dir_table.directory[j].inode_number;
                }
            }

            //check if file is already open
            for (int j = 0; j < MAX_NUMBER_OF_FILES; j++)
            {
                if (file_desc_table.fd[j].i_node != -1)
                {
                    if (file_inode == file_desc_table.fd[j].i_node)
                    {
                        perror("file is already open so cant open again");
                        return -1;
                    }
                }
            }

            //open the file
            file_fd = get_next_avail_fd();
            if (file_fd == -1)
            {
                perror("cant open more files");
                return -1;
            }
            file_desc_table.fd[file_fd].i_node = file_inode;

            //set the read pointer (to beginning of file) and write pointer (to end of file)
            file_desc_table.fd[file_fd].read_ptr = 0;
            file_desc_table.fd[file_fd].write_ptr = sfs_getfilesize(name);

            return file_fd;
        }
    }

    //file doesnt exist so need to create the file

    //get first free inode
    int free_inode = get_first_free_inode();
    if (free_inode == -1)
    {
        perror("no more free inodes left");
        return -1;
    }

    //get first free data block
    // int block = get_first_free_block();
    // if (block == -1)
    // {
    //     perror("no more blocks left");
    //     return -1;
    // }

    //get first free fd
    int fd = get_next_avail_fd();
    if (fd == -1)
    {
        perror("no more file descriptors available");
        return -1;
    }
    file_desc_table.fd[fd].i_node = free_inode;
    file_desc_table.fd[fd].read_ptr = 0;
    file_desc_table.fd[fd].write_ptr = 0;

    //set the inode
    inode_set(free_inode, -1, 0, -1, -1, 0, 0, 0);

    //create the file
    file_entry new_file;
    new_file.inode_number = free_inode;
    strncpy(new_file.filename, name, MAXFILENAME);

    //add to directory
    int dir_index = get_first_free_dir_index();
    if (dir_index == -1)
    {
        perror("directory is full");
        return -1;
    }
    dir_table.directory[dir_index].inode_number = free_inode;
    strncpy(dir_table.directory[dir_index].filename, new_file.filename, MAXFILENAME);

    return fd;
}

//--------------------------------------

int sfs_fclose(int fileID) //closes the given file
{
    if (file_desc_table.fd[fileID].i_node == -1)
    {
        perror("file isnt open so cant close it");
        return -1;
    }
    int inode = file_desc_table.fd[fileID].i_node;
    delete_inode(inode);
    file_desc_table.fd[fileID].i_node = -1;
    file_desc_table.fd[fileID].read_ptr = -1;
    file_desc_table.fd[fileID].write_ptr = -1;
    return 0; //success
}

//--------------------------------------

int sfs_frseek(int fileID, int loc) // seek (Read) to the location from beginning
{
    int fd = file_desc_table.fd[fileID].i_node;
    if (fd == -1)
    {
        perror("file not open");
        return -1;
    }
    int file_inode = get_inode_from_fileID(fileID);
    if (inodes[file_inode].size > loc)
    {
        perror("trying to set read pointer outside of file size");
        return -1;
    }
    file_desc_table.fd[fileID].read_ptr = loc;
    return 0;
}

//--------------------------------------sfs_fwseek-------------------------------

int sfs_fwseek(int fileID, int loc) // seek (Write) to the location from beginning
{
    int fd = file_desc_table.fd[fileID].i_node;
    if (fd == -1)
    {
        perror("file not open");
        return -1;
    }
    int file_inode = get_inode_from_fileID(fileID);
    if (inodes[file_inode].size > loc)
    {
        perror("trying to set write pointer outside of file size");
        return -1;
    }
    file_desc_table.fd[fileID].write_ptr = loc;
    return 0;
}

//--------------------------------------sfs_fwrite----------------------------------------

// int sfs_fwrite(int fileID, char *buf, int length) // write buf characters into disk
// {
//     int bytes_written = 0;
//     int bytes_left_to_write = length;
//     //get file inode
//     int file_inode = get_inode_from_fileID(fileID);
//     //get write pointer
//     int write_pointer = get_write_pointer_from_fileID(fileID);
//     printf("write ptr %d \n", write_pointer);

//     //inodes[file_inode].block_pointer[0];

//     //printf("id of file is %d  \n", fileID);

//     int num_blocks_owned = get_num_blocks_of_file(file_inode);
//     int curr_file_size = inodes[file_inode].size;
//     printf("curr file size: %d \n", curr_file_size);
//     int num_blocks_needed = ((curr_file_size + length) - num_blocks_owned * 1024) / 1024;

//     int num_blocks_remaining = get_num_free_blocks_remaining();
//     if (num_blocks_needed > num_blocks_remaining)
//     {
//         perror("cant perform this write because not enough blocks left");
//         return -1;
//     }
//     printf("size of file is %d   size want to write is %d   num blocks owned is %d  num blocks needed is %d\n", curr_file_size, length, num_blocks_owned, num_blocks_needed);
//     //need new blocks
//     if (curr_file_size + length > num_blocks_owned * 1024 && (num_blocks_needed > 0))
//     {

//         // if (num_blocks_owned + num_blocks_needed > 12)
//         // {

//         //     printf("id of file is %d   ", fileID);
//         printf("size of file is %d   size want to write is %d   num blocks owned is %d  num blocks needed is %d\n", curr_file_size, length, num_blocks_owned, num_blocks_needed);
//         //     perror("need more than 12 blocks");
//         //     return -1;
//         // }

//         if ((((curr_file_size + length) - num_blocks_owned * 1024) % 1024) != 0)
//         {
//             num_blocks_needed += 1;
//         }

//         //check if indirect block is needed
//         //int is_indirect_block_needed = 0; //set to false
//         int num_indirect_blocks_needed = 0;
//         if (num_blocks_needed + num_blocks_owned > 12)
//         {
//             // is_indirect_block_needed = 1; //set to true
//             num_indirect_blocks_needed = (num_blocks_needed + num_blocks_owned - 12);
//         }
//         int num_normal_blocks_needed = num_blocks_needed - num_indirect_blocks_needed;

//         // printf(" \n \t want to write %d bytes \n \t normal blocks needed %d \n \t indirect blcoks needed %d \n \t curr file size %d, write pointer %d \n", length, num_normal_blocks_needed, num_indirect_blocks_needed, curr_file_size, write_pointer);

//         //make the pointers point to the new blocks
//         for (int i = num_blocks_owned; i < num_blocks_owned + num_normal_blocks_needed; i++)
//         {
//             inodes[file_inode].block_pointer[i] = get_first_free_block();
//         }
//         //write in last original block if bytes available
//         int bytes_left_in_original_blocks = (num_blocks_owned * 1024 - curr_file_size);
//         if (bytes_left_in_original_blocks > 0)
//         {
//             //write in leftover block. Can only write to block from start of block so need to read the block then add partial buf
//             int last_og_block = inodes[file_inode].block_pointer[num_blocks_owned - 1];
//             char *temp_buff = (char *)malloc(sizeof(char) * 1024);
//             read_blocks(last_og_block, 1, temp_buff);
//             memcpy(temp_buff + (1024 - bytes_left_in_original_blocks), buf, bytes_left_in_original_blocks);
//             //  memcpy(temp_buff, buf, bytes_left_in_original_blocks);
//             write_blocks(last_og_block, 1, temp_buff);
//             bytes_written = bytes_left_in_original_blocks;
//             bytes_left_to_write -= bytes_written;
//             if (temp_buff != NULL)
//             {
//                 free(temp_buff);
//             }
//         }
//         //write in new block(s)
//         if (num_blocks_owned < 13)
//         {
//             for (int i = num_blocks_owned; i < num_blocks_owned + num_normal_blocks_needed; i++)
//             {
//                 char *temp_buf = (char *)malloc(sizeof(char) * 1024);
//                 memcpy(temp_buf, buf + bytes_written, 1024);
//                 write_blocks(inodes[file_inode].block_pointer[i], 1, temp_buf);
//                 //check if writing 1024 bytes
//                 if (bytes_left_to_write - 1024 >= 0)
//                 {
//                     bytes_written += 1024;
//                     bytes_left_to_write -= 1024;
//                     if (temp_buf != NULL)
//                     {
//                         free(temp_buf);
//                     }
//                 }
//                 else
//                 {
//                     bytes_written += bytes_left_to_write;
//                     bytes_left_to_write = 0;
//                     //update file size
//                     inodes[file_inode].size += bytes_written;

//                     //update write ptr
//                     file_desc_table.fd[fileID].write_ptr = inodes[file_inode].size;
//                     //return num bytes written
//                     if (temp_buf != NULL)
//                     {
//                         free(temp_buf);
//                     }
//                     return bytes_written;
//                 }
//             }
//         }

//         if (bytes_written == length)
//         {
//             inodes[file_inode].size += bytes_written;
//             file_desc_table.fd[fileID].write_ptr = inodes[file_inode].size;
//             return bytes_written;
//         }

//         //indirect block
//         if (num_indirect_blocks_needed > 0)
//         {
//             indirect_block_pointer indirect_block;
//             //check if indirect ptr is already assigned a block
//             if (inodes[file_inode].ind_pointer == -1)
//             {
//                 //need to initialize block to the indirect pointer
//                 inodes[file_inode].ind_pointer = get_first_free_block();
//                 if (inodes[file_inode].ind_pointer == -1)
//                 {
//                     perror("no more blocks left");
//                     return -1;
//                 }
//                 //initialize the indirect pointer block
//                 for (int i = 0; i < 128; i++)
//                 {
//                     indirect_block.ind_block[i] = -1;
//                 }
//             }
//             //else load the indirect pointer
//             else
//             {
//                 read_blocks(inodes[file_inode].ind_pointer, 1, &indirect_block);
//             }
//             //get block that write pointer is in
//             int curr_ind_block_index = (curr_file_size + bytes_written) / 1024 - 12; //curr_file_size is original size before function start

//             //get num of NEW ind blocks needed (including partial write)
//             int num_new_ind_blocks_needed = num_indirect_blocks_needed - curr_ind_block_index;

//             for (int i = curr_ind_block_index; i < num_new_ind_blocks_needed + curr_ind_block_index; i++)
//             {

//                 // printf("\n in indirect block code, curr ind block index %d, num of new ind block req is %d", curr_ind_block_index, num_new_ind_blocks_needed);
//                 // printf(" bytes written so far: %d, length: %d, bytes left towrite: %d \n", bytes_written, length, bytes_left_to_write);
//                 // printf("block num being pointed to: %d\n", indirect_block.ind_block[curr_ind_block_index]);

//                 //check if index points to a block already, if yes then write to partial block
//                 if (indirect_block.ind_block[curr_ind_block_index] != -1)
//                 {
//                     int bytes_left_in_curr_block = ((12 + curr_ind_block_index + 1) * 1024 - (curr_file_size + bytes_written));
//                     if (bytes_left_in_curr_block > 0)
//                     {
//                         //write in leftover block. Can only write to block from start of block so need to read the block then add partial buf
//                         int curr_ind_block = indirect_block.ind_block[curr_ind_block_index];
//                         char *temp_buff = (char *)malloc(sizeof(char) * 1024);
//                         read_blocks(curr_ind_block, 1, temp_buff);
//                         memcpy(temp_buff + (1024 - bytes_left_in_curr_block), buf + bytes_written, bytes_left_in_curr_block);
//                         write_blocks(curr_ind_block, 1, temp_buff);
//                         bytes_written = bytes_left_in_curr_block;
//                         bytes_left_to_write -= bytes_written;
//                         if (temp_buff != NULL)
//                         {
//                             free(temp_buff);
//                         }
//                     }
//                 }
//                 //else assign new block and write
//                 else
//                 {
//                     indirect_block.ind_block[curr_ind_block_index] = get_first_free_block();
//                     //////////
//                     if (indirect_block.ind_block[curr_ind_block_index] == -1)
//                     {
//                         perror("no more blocks left");
//                         return -1;
//                     }
//                     printf("block num being pointed to: %d\n", indirect_block.ind_block[curr_ind_block_index]);
//                     char *temp_buf = (char *)malloc(sizeof(char) * 1024);
//                     memcpy(temp_buf, buf + bytes_written, 1024);
//                     write_blocks(indirect_block.ind_block[curr_ind_block_index], 1, temp_buf);
//                     //check if writing 1024 bytes
//                     if (bytes_left_to_write - 1024 >= 0)
//                     {
//                         bytes_written += 1024;
//                         bytes_left_to_write -= 1024;
//                         if (temp_buf != NULL)
//                         {
//                             free(temp_buf);
//                         }
//                         if (bytes_left_to_write == 0)
//                         {
//                             inodes[file_inode].size = curr_file_size + bytes_written;
//                             file_desc_table.fd[fileID].write_ptr = inodes[file_inode].size;
//                             return bytes_written;
//                         }
//                     }
//                     else
//                     {
//                         bytes_written += bytes_left_to_write;
//                         bytes_left_to_write = 0;
//                         //update file size
//                         inodes[file_inode].size = curr_file_size + bytes_written;

//                         //update write ptr
//                         file_desc_table.fd[fileID].write_ptr = inodes[file_inode].size;
//                         //return num bytes written
//                         if (temp_buf != NULL)
//                         {
//                             free(temp_buf);
//                         }
//                         return bytes_written;
//                     }
//                 }
//             }
//         }

//         // else
//         // {

//         //     return -1;
//         // }
//         // //dont need to allocate new blocks
//         if (num_blocks_needed == 0)
//         {
//             printf("yeet/n");

//             char *temp_buf = (char *)malloc(sizeof(char) * 1024);
//             int last_og_block = inodes[file_inode].block_pointer[num_blocks_owned - 1];
//             int bytes_left_in_original_blocks = (num_blocks_owned * 1024 - curr_file_size);
//             read_blocks(last_og_block, 1, temp_buf);
//             memcpy(temp_buf + (1024 - bytes_left_in_original_blocks), buf, bytes_left_in_original_blocks);
//             write_blocks(last_og_block, 1, temp_buf);
//             bytes_written = length;
//             bytes_left_to_write = 0;
//             if (temp_buf != NULL)
//             {
//                 free(temp_buf);
//             }
//             file_desc_table.fd[fileID].write_ptr = inodes[file_inode].size;
//             return bytes_written;
//         }
//     }
//     //dont need to allocate new blocks
//     if (num_blocks_needed == 0 && (curr_file_size <= 12288 - length) && num_blocks_owned <= 12)
//     {
//         printf("yeet/n");
//         if (num_blocks_owned == 0)
//         {
//             inodes[file_inode].block_pointer[0] = get_first_free_block();
//             if (inodes[file_inode].block_pointer[0] == -1)
//             {
//                 perror("no blocks left");
//                 return -1;
//             }
//             char *temp_buf = (char *)malloc(sizeof(char) * 1024);
//             memcpy(temp_buf, buf, 1024);
//             write_blocks(inodes[file_inode].block_pointer[0], 1, temp_buf);
//             bytes_written = length;
//             bytes_left_to_write = 0;
//             if (temp_buf != NULL)
//             {
//                 free(temp_buf);
//             }
//             inodes[file_inode].size = curr_file_size + bytes_written;
//             file_desc_table.fd[fileID].write_ptr = inodes[file_inode].size;
//             printf("writting less than 1024 and getting first block ever, length %d, bytes written %d", length, bytes_written);
//             return bytes_written;
//         }
//     }
// }

//
//
//
//

int sfs_fwrite(int fileID, char *buf, int length) // write buf characters into disk
{
    
    
    int bytes_written = 0;
    int bytes_left_to_write = length;
    //get file inode
    int file_inode = get_inode_from_fileID(fileID);
    //get write pointer
    int write_pointer = get_write_pointer_from_fileID(fileID);
   // printf("write ptr %d \n", write_pointer);

    //inodes[file_inode].block_pointer[0];

    //printf("id of file is %d  \n", fileID);

    int num_blocks_owned = get_num_blocks_of_file(file_inode);
    int curr_file_size = inodes[file_inode].size;
   // printf("curr file size: %d \n", curr_file_size);

   if (curr_file_size + length > (12+120*1024)){
       perror("max file size reached");
       return -1;
   }
    int num_blocks_needed = ((curr_file_size + length) - num_blocks_owned * 1024) / 1024;

    int num_blocks_remaining = get_num_free_blocks_remaining();
    if (num_blocks_needed > num_blocks_remaining)
    {
        perror("cant perform this write because not enough blocks left");
        return -1;
    }

    while (bytes_left_to_write > 0)
    {
        int write_pointer = file_desc_table.fd[fileID].write_ptr;

        //get block index
        int block_index = write_pointer / 1024;

        //printf("block index %d", block_index);

        //get position of write pointer within the block
        int position = (block_index + 1) * 1024 - (write_pointer);
        position = 1024 - position;

        if (block_index < 12)
        {
            //using regular blocks

            //get block number
            int block_num = inodes[file_inode].block_pointer[block_index];

            //check if need new block
            if (block_num == -1)
            {
                block_num = get_first_free_block();
                if (block_num == -1)
                {
                    perror("no more blocks left");
                    return -1;
                }
                //add to inode
                inodes[file_inode].block_pointer[block_index] = block_num;
            }
            //check if doing full write
            if (position == 0)
            {
                char *temp_buf = (char *)malloc(sizeof(char) * 1024);
                memcpy(temp_buf, buf + bytes_written, 1024);
                write_blocks(inodes[file_inode].block_pointer[block_index], 1, temp_buf);
                if (bytes_left_to_write < 1024)
                {
                    bytes_written += bytes_left_to_write;
                    inodes[file_inode].size = curr_file_size + bytes_written;
                    file_desc_table.fd[fileID].write_ptr = inodes[file_inode].size;
                    if (temp_buf != NULL)
                    {
                        free(temp_buf);
                    }
                    return bytes_written;
                }
                bytes_written += 1024;
                bytes_left_to_write -= 1024;
                inodes[file_inode].size = curr_file_size + bytes_written;
                file_desc_table.fd[fileID].write_ptr = inodes[file_inode].size;
                if (temp_buf != NULL)
                {
                    free(temp_buf);
                }
            }
            else
            {
                //doing write in middle of block
                //get bytes available in curr block
                int bytes_avail_in_curr_block = (block_index + 1) * 1024 - write_pointer;

                char *temp_buf = (char *)malloc(sizeof(char) * 1024);
                //read whats currently in the block
                read_blocks(inodes[file_inode].block_pointer[block_index], 1, temp_buf);
                memcpy(temp_buf + (1024 - bytes_avail_in_curr_block), buf + bytes_written, bytes_avail_in_curr_block);
                write_blocks(inodes[file_inode].block_pointer[block_index], 1, temp_buf);

                if (bytes_left_to_write < bytes_avail_in_curr_block)
                {
                    bytes_written += bytes_left_to_write;
                    bytes_left_to_write = 0;
                    if (temp_buf != NULL)
                    {
                        free(temp_buf);
                    }
                    inodes[file_inode].size = curr_file_size + bytes_written;
                    file_desc_table.fd[fileID].write_ptr = inodes[file_inode].size;
                    return bytes_written;
                }

                bytes_written += bytes_avail_in_curr_block;
                bytes_left_to_write -= bytes_avail_in_curr_block;
                if (temp_buf != NULL)
                {
                    free(temp_buf);
                }
                inodes[file_inode].size = curr_file_size + bytes_written;
                file_desc_table.fd[fileID].write_ptr = inodes[file_inode].size;
            }
        }
        else
        {
            //using indirect blocks
            indirect_block_pointer indirect_block;
            //check if indirect ptr is already assigned a block
            if (inodes[file_inode].ind_pointer == -1)
            {
                //need to initialize block to the indirect pointer
                inodes[file_inode].ind_pointer = get_first_free_block();
                if (inodes[file_inode].ind_pointer == -1)
                {
                    perror("no more blocks left");
                    return -1;
                }
                //initialize the indirect pointer block
                for (int i = 0; i < 128; i++)
                {
                    indirect_block.ind_block[i] = -1;
                }
            }
            //else load the indirect pointer
            else
            {
                read_blocks(inodes[file_inode].ind_pointer, 1, &indirect_block);
            }
            //get block that write pointer is in
            int curr_ind_block_index = (curr_file_size + bytes_written) / 1024 - 12; //curr_file_size is original size before function start

            //  check if index points to a block already, if yes then write to partial block
            if (indirect_block.ind_block[curr_ind_block_index] != -1)
            {
                int bytes_left_in_curr_block = ((12 + curr_ind_block_index + 1) * 1024 - (curr_file_size + bytes_written));
                if (bytes_left_in_curr_block > 0)
                {
                    //write in leftover block. Can only write to block from start of block so need to read the block then add partial buf
                    int curr_ind_block = indirect_block.ind_block[curr_ind_block_index];
                    char *temp_buff = (char *)malloc(sizeof(char) * 1024);
                    read_blocks(curr_ind_block, 1, temp_buff);
                    memcpy(temp_buff + (1024 - bytes_left_in_curr_block), buf + bytes_written, bytes_left_in_curr_block);
                    write_blocks(curr_ind_block, 1, temp_buff);
                    bytes_written = bytes_left_in_curr_block;
                    bytes_left_to_write -= bytes_written;
                    if (temp_buff != NULL)
                    {
                        free(temp_buff);
                    }
                    inodes[file_inode].size = curr_file_size + bytes_written;
                    file_desc_table.fd[fileID].write_ptr = inodes[file_inode].size;
                }
            }
            //else need new block
            else{
                 indirect_block.ind_block[curr_ind_block_index] = get_first_free_block();
                    //////////
                    if (indirect_block.ind_block[curr_ind_block_index] == -1)
                    {
                        perror("no more blocks left");
                        return -1;
                    }
                  //  printf("block num being pointed to: %d\n", indirect_block.ind_block[curr_ind_block_index]);
                    char *temp_buf = (char *)malloc(sizeof(char) * 1024);
                    memcpy(temp_buf, buf + bytes_written, 1024);
                    write_blocks(indirect_block.ind_block[curr_ind_block_index], 1, temp_buf);
                    //check if writing 1024 bytes
                    if (bytes_left_to_write - 1024 >= 0)
                    {
                        bytes_written += 1024;
                        bytes_left_to_write -= 1024;
                        if (temp_buf != NULL)
                        {
                            free(temp_buf);
                        }
                        inodes[file_inode].size = curr_file_size + bytes_written;
                    file_desc_table.fd[fileID].write_ptr = inodes[file_inode].size;
                        if (bytes_left_to_write == 0)
                        {
                            inodes[file_inode].size = curr_file_size + bytes_written;
                            file_desc_table.fd[fileID].write_ptr = inodes[file_inode].size;
                            return bytes_written;
                        }


                    }
                    else {
                                 bytes_written += bytes_left_to_write;
                        bytes_left_to_write = 0;
                        //update file size
                        inodes[file_inode].size = curr_file_size + bytes_written;

                        //update write ptr
                        file_desc_table.fd[fileID].write_ptr = inodes[file_inode].size;
                        //return num bytes written
                        if (temp_buf != NULL)
                        {
                            free(temp_buf);
                        }
                        return bytes_written;
                    }
            }


        }
    }
    return bytes_written;
}

//
//
//
//

//--------------------------------------sfs_read---------------------------------------------

int sfs_fread(int fileID, char *buf, int length) // read characters from disk into buf
{
    int bytes_left_to_read = length;
    int bytes_read = 0;

    //get read pointer
    int read_pointer = file_desc_table.fd[fileID].read_ptr;

    //check if file is open
    if (file_desc_table.fd[fileID].i_node == -1)
    {
        perror("cant read the file. file isnt open");
        return -1;
    }
    //get inode
    int inode = file_desc_table.fd[fileID].i_node;

    // if (inodes[inode].size < length)
    // {
    //     perror("file is smaller than amount you want to read");
    //     return -1;
    // }

    if (read_pointer == 0)
    {
        if (length < 1024)
        {
            //create temporary buffer that will store the whole contents of the last block
            char *temp_buf = (char *)malloc(sizeof(char) * 1024);
            read_blocks(inodes[inode].block_pointer[0], 1, temp_buf);

            //only take the required amount of bytes into buf
            memcpy(buf, temp_buf, length);
            bytes_read = length;
            if (temp_buf != NULL)
            {
                free(temp_buf);
            }
            file_desc_table.fd[fileID].read_ptr = read_pointer + bytes_read;
            return bytes_read;
        }

        //calculate how many blocks need to be read
        int num_full_blocks_to_read = (length / 1024);
        int num_not_full_block_to_read = 0;
        if (length % 1024 != 0)
        {
            num_full_blocks_to_read++;
        }
        //read into buf
        for (int i = 0; i < num_full_blocks_to_read - 1; i++)
        {
            int block_num = inodes[inode].block_pointer[i];
            read_blocks(block_num, 1, buf + bytes_read);
            bytes_read += 1024;
            bytes_left_to_read -= 1024;
            // printf("block number: %d    btes read: %d of %d  bytes left to read: %d\n", i, bytes_read, length, bytes_left_to_read);
        }
        // if (num_not_full_block_to_read ==1){
        if (bytes_left_to_read > 0)
        {
            // printf("in the part read section");
            //create temporary buffer that will store the whole contents of the last block
            char *temp_buf = (char *)malloc(sizeof(char) * 1024);
            read_blocks(inodes[inode].block_pointer[num_full_blocks_to_read], 1, temp_buf);

            //only take the required amount of bytes into buf
            memcpy(buf + bytes_read, temp_buf, bytes_left_to_read);
            bytes_read += bytes_left_to_read;
            if (temp_buf != NULL)
            {
                free(temp_buf);
            }
        }
        file_desc_table.fd[fileID].read_ptr = read_pointer + bytes_read;
        return bytes_read;
    }

    //read pointer is not at position 0
    int block_pointer_number = read_pointer / 1024;
    int read_pointer_block_num = -1;

    if (block_pointer_number < 12)
    {
        read_pointer_block_num = inodes[inode].block_pointer[block_pointer_number];
    }
    else if (block_pointer_number >= 12)
    {
      //  printf("their tester is trying to read from indirect pointer blocks rip\n");
        return -1;
    }

    //if length is contained within the block that read pointer is in
    if (length < (read_pointer_block_num + 1) * 1024 - read_pointer)
    {
        //create temporary buffer that will store the whole contents of the last block
        char *temp_buf = (char *)malloc(sizeof(char) * 1024);
        read_blocks(inodes[inode].block_pointer[block_pointer_number], 1, temp_buf);

        //only take the required amount of bytes into buf
        memcpy(buf, temp_buf + read_pointer, length);
        bytes_read = length;
        if (temp_buf != NULL)
        {
            free(temp_buf);
        }
        file_desc_table.fd[fileID].read_ptr = read_pointer + bytes_read;
        return bytes_read;
    }

    //calculate how many bytes of block that read_pointer is in needs to be read
    int num_bytes_before_read_full = (read_pointer_block_num + 1) * 1024 - read_pointer;
    if (num_bytes_before_read_full > 0)
    {
        //create temporary buffer that will store the whole contents of the last block
        char *temp_buf = (char *)malloc(sizeof(char) * 1024);
        read_blocks(inodes[inode].block_pointer[block_pointer_number], 1, temp_buf);

        //only take the required amount of bytes into buf
        memcpy(buf, temp_buf + read_pointer, length);
        bytes_read = length;
        if (temp_buf != NULL)
        {
            free(temp_buf);
        }
        bytes_read += num_bytes_before_read_full;
        bytes_left_to_read -= num_bytes_before_read_full;
    }

    //calculate how many full blocks to be read
    int num_full_blocks_to_read = ((length - bytes_read) / 1024);

    for (int i = 1; i < num_full_blocks_to_read + 1; i++)
    {
        int block_num = inodes[inode].block_pointer[block_pointer_number + i];
        read_blocks(block_num, 1, buf + bytes_read);
        bytes_read += 1024;
        bytes_left_to_read -= 1024;
    }

    if (bytes_left_to_read > 0)
    {
        // printf("in the part read section");
        //create temporary buffer that will store the whole contents of the last block
        char *temp_buf = (char *)malloc(sizeof(char) * 1024);
        read_blocks(inodes[inode].block_pointer[block_pointer_number + block_pointer_number + 2], 1, temp_buf);

        //only take the required amount of bytes into buf
        memcpy(buf + bytes_read, temp_buf, bytes_left_to_read);
        bytes_read += bytes_left_to_read;
        if (temp_buf != NULL)
        {
            free(temp_buf);
        }
    }
    file_desc_table.fd[fileID].read_ptr = read_pointer + bytes_read;
    return bytes_read;
}

//--------------------------------------sfs_remove-------------------------------------

int sfs_remove(char *file) // removes a file from the filesystem
{
    return -1;
}