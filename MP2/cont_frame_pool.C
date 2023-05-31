/*
 File: ContFramePool.C
 
 Author:
 Date  : 
 
 */

/*--------------------------------------------------------------------------*/
/* 
 POSSIBLE IMPLEMENTATION
 -----------------------

 The class SimpleFramePool in file "simple_frame_pool.H/C" describes an
 incomplete vanilla implementation of a frame pool that allocates 
 *single* frames at a time. Because it does allocate one frame at a time, 
 it does not guarantee that a sequence of frames is allocated contiguously.
 This can cause problems.
 
 The class ContFramePool has the ability to allocate either single frames,
 or sequences of contiguous frames. This affects how we manage the
 free frames. In SimpleFramePool it is sufficient to maintain the free 
 frames.
 In ContFramePool we need to maintain free *sequences* of frames.
 
 This can be done in many ways, ranging from extensions to bitmaps to 
 free-lists of frames etc.
 
 IMPLEMENTATION:
 
 One simple way to manage sequences of free frames is to add a minor
 extension to the bitmap idea of SimpleFramePool: Instead of maintaining
 whether a frame is FREE or ALLOCATED, which requires one bit per frame, 
 we maintain whether the frame is FREE, or ALLOCATED, or HEAD-OF-SEQUENCE.
 The meaning of FREE is the same as in SimpleFramePool. 
 If a frame is marked as HEAD-OF-SEQUENCE, this means that it is allocated
 and that it is the first such frame in a sequence of frames. Allocated
 frames that are not first in a sequence are marked as ALLOCATED.
 
 NOTE: If we use this scheme to allocate only single frames, then all 
 frames are marked as either FREE or HEAD-OF-SEQUENCE.
 
 NOTE: In SimpleFramePool we needed only one bit to store the state of 
 each frame. Now we need two bits. In a first implementation you can choose
 to use one char per frame. This will allow you to check for a given status
 without having to do bit manipulations. Once you get this to work, 
 revisit the implementation and change it to using two bits. You will get 
 an efficiency penalty if you use one char (i.e., 8 bits) per frame when
 two bits do the trick.
 
 DETAILED IMPLEMENTATION:
 
 How can we use the HEAD-OF-SEQUENCE state to implement a contiguous
 allocator? Let's look a the individual functions:
 
 Constructor: Initialize all frames to FREE, except for any frames that you 
 need for the management of the frame pool, if any.
 
 get_frames(_n_frames): Traverse the "bitmap" of states and look for a 
 sequence of at least _n_frames entries that are FREE. If you find one, 
 mark the first one as HEAD-OF-SEQUENCE and the remaining _n_frames-1 as
 ALLOCATED.

 release_frames(_first_frame_no): Check whether the first frame is marked as
 HEAD-OF-SEQUENCE. If not, something went wrong. If it is, mark it as FREE.
 Traverse the subsequent frames until you reach one that is FREE or 
 HEAD-OF-SEQUENCE. Until then, mark the frames that you traverse as FREE.
 
 mark_inaccessible(_base_frame_no, _n_frames): This is no different than
 get_frames, without having to search for the free sequence. You tell the
 allocator exactly which frame to mark as HEAD-OF-SEQUENCE and how many
 frames after that to mark as ALLOCATED.
 
 needed_info_frames(_n_frames): This depends on how many bits you need 
 to store the state of each frame. If you use a char to represent the state
 of a frame, then you need one info frame for each FRAME_SIZE frames.
 
 A WORD ABOUT RELEASE_FRAMES():
 
 When we releae a frame, we only know its frame number. At the time
 of a frame's release, we don't know necessarily which pool it came
 from. Therefore, the function "release_frame" is static, i.e., 
 not associated with a particular frame pool.
 
 This problem is related to the lack of a so-called "placement delete" in
 C++. For a discussion of this see Stroustrup's FAQ:
 http://www.stroustrup.com/bs_faq2.html#placement-delete
 
 */
/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "cont_frame_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */
// Frame Pool Linked List intialised to NULL
ContFramePool* ContFramePool::head = NULL; 

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   C o n t F r a m e P o o l */
/*--------------------------------------------------------------------------*/

ContFramePool::ContFramePool(unsigned long _base_frame_no,
                             unsigned long _n_frames,
                             unsigned long _info_frame_no)
{
    assert(_n_frames<=FRAME_SIZE*4);

    // Checking if the number of frames is divisible by 8 so as to 
    // be able to fill the bitmap
    assert((nframes%8)==0);
    
    // Initialize variables
    base_frame_no=_base_frame_no;
    nframes=_n_frames;
    nFreeFrames=_n_frames;
    info_frame_no=_info_frame_no;
    
    // If _info_frame_no is zero, by deafault we store management info in the first
    // frame, otherwise we use the _info_frame_no frame to keep management info
    if(info_frame_no==0) {
        bitmap=(unsigned char *)(base_frame_no*FRAME_SIZE);
    } else {
        bitmap=(unsigned char *)(info_frame_no*FRAME_SIZE);
    }
    
    
    // Marking all frames as free initially
    int i=0;
    while(i*4<_n_frames){
        bitmap[i]=0xFF;
        i++;
    }

    
    if(_info_frame_no == 0) {
        // Marking the first frame as allocated since first frame stores management info.
        // This is mark that this is being used for Kernel pool
        bitmap[0]=0x7F;
        nFreeFrames--;
    }

    //Creating a Linked List of the Pool with Kernel Pool as the HEAD
	if(head==NULL)
	{
		head=this;
		head->next=NULL;
	}
	else
	{
       ContFramePool *itr =NULL;
       for(itr=head;itr->next!=NULL;itr=itr->next);
       itr->next = this;
	   itr = this;
       itr->next = NULL;
	}	
    Console::puts("Frame Pool initialized\n");
}

unsigned long ContFramePool::get_frames(unsigned int _n_frames)
{

	    	
	//Checking if frames are free to allocate
	if(_n_frames <= nFreeFrames)
	{
	    Console::puts("The number of frames requested is more than the available number of frames.\n ");
		assert(false);
	}
		   
    unsigned int i=0,j=0;
	unsigned int start_frame_no=base_frame_no; 
	
	//Looping to check already allocated frames
	loop:
    while (bitmap[i]==0x7F || bitmap[i]==0x3F) {
		i++;
    }
	
    start_frame_no = base_frame_no + i;
    j=i;
	
	//Checking if there is a continuous segment of free frames available from the first free frame.
	while(j<_n_frames+i){
        if(!((start_frame_no+_n_frames)<=(base_frame_no+nframes)))
		{
			Console::puts("There are no continuous free frames available.\n");
			assert(false);
		}
		
		if((bitmap[j]==0x7F) || (bitmap[j]==0x3F))
		{
		   if(j<base_frame_no+nframes)
		   {
			   i=j;
		       goto loop; 		  // Looping to see the next available frame
		   }
		   else
		   {
			 Console::puts("ERROR ERROR ERROR !!! No continuous free frames available.\n");
			 assert(false);
		   }			  
		}
        j++;
    }
	markBits(i, _n_frames);
	return start_frame_no;
}

void ContFramePool::markBits(unsigned int i, unsigned int _n_frames)	
{
    //Mark that frame as being used in the bitmap.	

    unsigned char useMask=0x80;   
    for(unsigned int f=_n_frames;f!=0;f--){
        if(f==_n_frames)
        {
            bitmap[i]=bitmap[i]^0xC0;  //This is to mark the head
        }else
        {
            bitmap[i] = bitmap[i]^useMask;  //This is to mark allocated frame.
        }
        nFreeFrames--;
        i++;
    }
}

void ContFramePool::mark_inaccessible(unsigned long _base_frame_no,
                                      unsigned long _n_frames)
{
    for(unsigned int i=_base_frame_no; i<_base_frame_no+_n_frames;i++){

        if(!((i>=base_frame_no) && (i<base_frame_no+nframes)))
        {
            Console::puts("The range provided is out of bounds. Please check.\n");
            assert(false);
        }
        
        unsigned int index = i - base_frame_no;
        unsigned char useMask = 0xC0;
        
        assert((bitmap[index] & useMask) != 0); // If it's already in use, this is an invalid state.
        
        bitmap[index] ^= useMask; // Update the index in bitmap
    }
    nFreeFrames-=_n_frames;
}

void ContFramePool::release_frames(unsigned long _first_frame_no)
{
	for(ContFramePool *itr=head;itr!= NULL;itr=itr->next)
	{
		 if((_first_frame_no >= itr->base_frame_no )&&(_first_frame_no <= ( itr->base_frame_no + itr->nframes - 1)))
		 {
			itr->release_a_frame(_first_frame_no);
            break;		
		 }	 				
	
	}
}

void ContFramePool::release_a_frame(unsigned long _first_frame_no )
{
    if(!(bitmap[_first_frame_no-base_frame_no]==0x3F))
    {
    Console::puts("This is not a head frame. So cannot release frame.");
    assert(false);	
    }
    for(unsigned int i=_first_frame_no-base_frame_no; i<nframes ;)
    {
            bitmap[i]|=0xC0; // resetting the bitmaps	   
            i++; 
            nFreeFrames++; // Increasing the number of Free Frames
            
            if((bitmap[i] ^ 0xFF)==0x80) 
                continue;// Here, we continue to deallocate as we haven't found the next HEAD frame yet.
            else
                break; //Found the next HEAD frame, so stop deallocating.
    }
}

unsigned long ContFramePool::needed_info_frames(unsigned long _n_frames)
{
   //Assuming 8 bits per frame
   return ((_n_frames *8) /(4*1024*8)) + (((_n_frames*8) % (4*1024*8)) >0 ? 1 : 0 );
}
