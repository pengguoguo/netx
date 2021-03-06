/**************************************************************************/
/*                                                                        */
/*       Copyright (c) Microsoft Corporation. All rights reserved.        */
/*                                                                        */
/*       This software is licensed under the Microsoft Software License   */
/*       Terms for Microsoft Azure RTOS. Full text of the license can be  */
/*       found in the LICENSE file at https://aka.ms/AzureRTOS_EULA       */
/*       and in the root directory of this software.                      */
/*                                                                        */
/**************************************************************************/


/**************************************************************************/
/**************************************************************************/
/**                                                                       */
/** NetX Component                                                        */
/**                                                                       */
/**   Internet Protocol (IP)                                              */
/**                                                                       */
/**************************************************************************/
/**************************************************************************/

#define NX_SOURCE_CODE


/* Include necessary system files.  */

#include "nx_api.h"
#include "tx_thread.h"
#include "nx_ip.h"
#include "nx_packet.h"


/**************************************************************************/
/*                                                                        */
/*  FUNCTION                                               RELEASE        */
/*                                                                        */
/*    _nx_ip_raw_packet_processing                        PORTABLE C      */
/*                                                           6.0          */
/*  AUTHOR                                                                */
/*                                                                        */
/*    Yuxin Zhou, Microsoft Corporation                                   */
/*                                                                        */
/*  DESCRIPTION                                                           */
/*                                                                        */
/*    This function processes a received raw IP packet from the           */
/*    _nx_ip_packet_receive function and either queues it or gives it to  */
/*    the first suspended thread waiting for a raw IP packet.             */
/*                                                                        */
/*  INPUT                                                                 */
/*                                                                        */
/*    ip_ptr                                Pointer to IP control block   */
/*    packet_ptr                            Pointer to packet to send     */
/*                                                                        */
/*  OUTPUT                                                                */
/*                                                                        */
/*    None                                                                */
/*                                                                        */
/*  CALLS                                                                 */
/*                                                                        */
/*    _tx_thread_system_resume              Resume suspended thread       */
/*                                                                        */
/*  CALLED BY                                                             */
/*                                                                        */
/*    _nx_ip_packet_receive                 Packet receive processing     */
/*                                                                        */
/*  RELEASE HISTORY                                                       */
/*                                                                        */
/*    DATE              NAME                      DESCRIPTION             */
/*                                                                        */
/*  05-19-2020     Yuxin Zhou               Initial Version 6.0           */
/*                                                                        */
/**************************************************************************/
VOID  _nx_ip_raw_packet_processing(NX_IP *ip_ptr, NX_PACKET *packet_ptr)
{

TX_INTERRUPT_SAVE_AREA
TX_THREAD *thread_ptr;



    /* Determine if there is a thread waiting for the IP packet.  If so, just
       give the packet to the waiting thread.  */
    thread_ptr =  ip_ptr -> nx_ip_raw_packet_suspension_list;
    if (thread_ptr)
    {

        /* Disable interrupts.  */
        TX_DISABLE

        /* Yes, a thread is suspended on the raw IP packet queue.  */

        /* See if this is the only suspended thread on the list.  */
        if (thread_ptr == thread_ptr -> tx_thread_suspended_next)
        {

            /* Yes, the only suspended thread.  */

            /* Update the head pointer.  */
            ip_ptr -> nx_ip_raw_packet_suspension_list =  NX_NULL;
        }
        else
        {

            /* At least one more thread is on the same expiration list.  */

            /* Update the list head pointer.  */
            ip_ptr -> nx_ip_raw_packet_suspension_list =  thread_ptr -> tx_thread_suspended_next;

            /* Update the links of the adjacent threads.  */
            (thread_ptr -> tx_thread_suspended_next) -> tx_thread_suspended_previous =
                thread_ptr -> tx_thread_suspended_previous;
            (thread_ptr -> tx_thread_suspended_previous) -> tx_thread_suspended_next =
                thread_ptr -> tx_thread_suspended_next;
        }

        /* Decrement the suspension count.  */
        ip_ptr -> nx_ip_raw_packet_suspended_count--;

        /* Prepare for resumption of the first thread.  */

        /* Clear cleanup routine to avoid timeout.  */
        thread_ptr -> tx_thread_suspend_cleanup =  TX_NULL;

        /* Temporarily disable preemption.  */
        _tx_thread_preempt_disable++;

        /* Restore interrupts.  */
        TX_RESTORE

        /* Return this packet pointer to the suspended thread waiting for
           a block.  */
        *((NX_PACKET **)thread_ptr -> tx_thread_additional_suspend_info) =  packet_ptr;

        /* Put return status into the thread control block.  */
        thread_ptr -> tx_thread_suspend_status =  NX_SUCCESS;

        /* Resume thread.  */
        _tx_thread_system_resume(thread_ptr);
    }
    else
    {

        /* Disable interrupts.  */
        TX_DISABLE

        /* Otherwise, queue the raw IP packet in a FIFO manner on the raw packet list.  */

        /* Clear the next packet pointer.  */
        packet_ptr -> nx_packet_queue_next =  NX_NULL;

        /* Determine if the queue is empty.  */
        if (ip_ptr -> nx_ip_raw_received_packet_tail)
        {

            /* List is not empty, place the raw packet at the end of the raw packet list.  */
            (ip_ptr -> nx_ip_raw_received_packet_tail) -> nx_packet_queue_next =  packet_ptr;
            ip_ptr -> nx_ip_raw_received_packet_tail = packet_ptr;
        }
        else
        {

            /* This is the first entry on the queue so set the head and tail pointers.  */
            ip_ptr -> nx_ip_raw_received_packet_head =  packet_ptr;
            ip_ptr -> nx_ip_raw_received_packet_tail =  packet_ptr;
        }

        /* Increment the raw packet received count.  */
        ip_ptr -> nx_ip_raw_received_packet_count++;

        /* Restore interrupts.  */
        TX_RESTORE
    }
}

