/*
 * Copyright 2002-2003. The Regents of the University of
 * California. This material was produced under U.S. Government
 * contract W-7405-ENG-36 for Los Alamos National Laboratory, which is
 * operated by the University of California for the U.S. Department of
 * Energy. The Government is granted for itself and others acting on
 * its behalf a paid-up, nonexclusive, irrevocable worldwide license
 * in this material to reproduce, prepare derivative works, and
 * perform publicly and display publicly. Beginning five (5) years
 * after October 10,2002 subject to additional five-year worldwide
 * renewals, the Government is granted for itself and others acting on
 * its behalf a paid-up, nonexclusive, irrevocable worldwide license
 * in this material to reproduce, prepare derivative works, distribute
 * copies to the public, perform publicly and display publicly, and to
 * permit others to do so. NEITHER THE UNITED STATES NOR THE UNITED
 * STATES DEPARTMENT OF ENERGY, NOR THE UNIVERSITY OF CALIFORNIA, NOR
 * ANY OF THEIR EMPLOYEES, MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR
 * ASSUMES ANY LEGAL LIABILITY OR RESPONSIBILITY FOR THE ACCURACY,
 * COMPLETENESS, OR USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT,
 * OR PROCESS DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE
 * PRIVATELY OWNED RIGHTS.

 * Additionally, this program is free software; you can distribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or any later version.  Accordingly, this
 * program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */
/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#ifndef _VECTOR_
#define _VECTOR_

#include <new>
#include <strings.h>
#include "internal/system.h"
#include "internal/new.h"
#include "internal/malloc.h"


////////////////////////////////////////////////////////////////////////////
//
//  Vector
//

template<class TYPE>
class Vector {
public:
 
   const size_t BLOCK_SIZE;  

   Vector();
   Vector(const Vector<TYPE>&);
  ~Vector();

   size_t size() const;

   // index operations
   TYPE  operator[](size_t) const;
   TYPE& operator[](size_t);

   TYPE* base() { return ptr; }
   const TYPE* base() const { return ptr; }
    
   int compare(const Vector<TYPE>&) const;
   int compare(const Vector<TYPE>&, size_t indices) const;

   // insert at any location
   bool insert(size_t index, TYPE, size_t count = 1);
   bool insert(size_t index, const TYPE*, size_t);
   bool insert(size_t index, const Vector<TYPE>&);
   bool insert(size_t index, const Vector<TYPE>&, size_t count);

   // remove indices from the array
   bool remove(size_t index, size_t count = 1);
   void remove_all();

   // comparison
   bool operator==(const Vector<TYPE>&) const;
   bool operator< (const Vector<TYPE>&) const;

   // assignment
   Vector& operator=(const Vector<TYPE>&);
   Vector& operator=(const TYPE&);

   // append
   Vector& operator+=(const Vector<TYPE>&);
   Vector& operator+=(TYPE);

   bool push_back(TYPE);
   bool push_back(const Vector<TYPE>&);
   bool push_back(const Vector<TYPE>&, size_t count);

   bool   size(size_t);
   bool   reserve(size_t);
   size_t reserved() const { return allocated; }

protected:
   size_t len;
   TYPE  *ptr;
   size_t allocated;
};


template<class TYPE>
Vector<TYPE>::Vector() :
   BLOCK_SIZE(16),
   len(0),
   ptr(0),
   allocated(0)
   {
   }


template<class TYPE>
Vector<TYPE>::Vector(const Vector<TYPE>& array) :
   BLOCK_SIZE(16),
   len(0),
   ptr(0),
   allocated(0)
   {
   *this += array;
   }


template<class TYPE>
Vector<TYPE>::~Vector()
   {
   remove_all();
   }


/////////////////////////////////////////////////////////////////////////////
//
//  operator+=
//

template<class TYPE>
Vector<TYPE>& Vector<TYPE>::operator+=(const Vector<TYPE>& array)
   {
   insert(len, array);
   return *this;
   }

template<class TYPE>
Vector<TYPE>& Vector<TYPE>::operator+=(TYPE item)
   {
   insert(len, item);
   return *this;
   }

template<class TYPE>
bool Vector<TYPE>::push_back(TYPE item)
   {
   return insert(len, item);
   }

template<class TYPE>
bool Vector<TYPE>::push_back(const Vector<TYPE>& array)
   {
   return insert(len, array, array.len);
   }

template<class TYPE>
bool Vector<TYPE>::push_back(const Vector<TYPE>& array, size_t count)
   {
   return insert(len, array, count);
   }


/////////////////////////////////////////////////////////////////////////////
//
//  operator=
//

template<class TYPE>
Vector<TYPE>& Vector<TYPE>::operator=(const Vector<TYPE>& array)
   {
   if(size(array.len) == false)
       exit(-1);
   for(size_t i=0; i<array.len; i++)
       ptr[i] = array[i];
   return *this;
   }

template<class TYPE>
Vector<TYPE>& Vector<TYPE>::operator=(const TYPE& element)
   {
   if(size(1) == false)
      exit(-1);
   ptr[0] = element;
   return *this;
   }


/////////////////////////////////////////////////////////////////////////////
//
//  operator[]
//

template<class TYPE>
TYPE& Vector<TYPE>::operator[](size_t index)
   {
#if !defined(NDEBUG)
   if(index >= len)
       ulm_exit((-1, "Vector<TYPE>::operator[](%d) invalid index", index));
#endif
   return ptr[index];
   }

template<class TYPE>
TYPE Vector<TYPE>::operator[](size_t index) const
   {
#if !defined(NDEBUG)
   if(index >= len) 
       ulm_exit((-1, "Vector<TYPE>::operator[](%d) invalid index", index));
#endif
   return ptr[index];
   }



/////////////////////////////////////////////////////////////////////////////
//
//  compare()
//
//  Returns
//   -1  this is lexically less
//    0  same
//    1  greater than
//

template<class TYPE>
int Vector<TYPE>::compare(const Vector<TYPE>& array) const
   {
   return compare(array, (len > array.len) ? len : array.len);
   }

template<class TYPE>
int Vector<TYPE>::compare(const Vector<TYPE>& array, size_t count) const
   {
   if(count-- == 0)
      return 0;
   for(unsigned int i=0; i<len; i++)
      {
      if(i>=array.len || ptr[i] > array.ptr[i])
         return 1;
      if(ptr[i] < array.ptr[i])
         return -1;
      if(i == count)
         return 0;
      }
   if(len < array.len)
      return -1;
   return 0;
   }

template<class TYPE>
bool Vector<TYPE>::operator==(const Vector<TYPE>& array) const
   {
   return (compare(array, len > array.len ? len : array.len) == 0) ? true : false;
   }

template<class TYPE>
bool Vector<TYPE>::operator<(const Vector<TYPE>& array) const
   {
   return (compare(array, len > array.len ? len : array.len) < 0) ? true : false;
   }


/////////////////////////////////////////////////////////////////////////////
//
//  insert()
//

template<class TYPE>
bool Vector<TYPE>::insert(size_t index, TYPE value, size_t count)
   {
   if(count == 0)
      return true;

   size_t new_len = (index >= len) ? index+count : len+count;
   if(reserve(new_len) == false)
      return false;

   // construct new items
   for(size_t i=len; i<new_len; i++)
       new(ptr+i) TYPE;

   // if this index already exists, shift everything
   if(index < len)
      {
      register size_t num_items = len - index;
      register TYPE *src = &ptr[len-1];
      register TYPE *dst = &src[count];
      while(num_items-- != 0)
         *dst-- = *src--;
      }

   // fill in the requested value
   for(size_t i=index; i<index+count; i++)
      ptr[i] = value;
   len = new_len;
   return true;
   }

template<class TYPE>
bool Vector<TYPE>::insert(size_t index, const Vector<TYPE>& array)
   {
   return insert(index, array, array.len);
   }

template<class TYPE>
bool Vector<TYPE>::insert(size_t index, const Vector<TYPE>& array, size_t count)
   {
   if(count > array.len)
      return false;
   return insert(index, array.ptr, count);
   }

template<class TYPE>
bool Vector<TYPE>::insert(size_t index, const TYPE* array, size_t count)
   {
   if(count == 0)
      return true;
   size_t new_len = (index >= len) ? index+count : len+count;
   if(reserve(new_len) == false)
      return false;
   for(size_t i=len; i<new_len; i++)
       new(ptr+i) TYPE;

   // if this index already exists, shift everything up by count
   if(index < len)
      {
      register size_t num_items = len - index;
      register TYPE *src = &ptr[len-1];
      register TYPE *dst = &src[count];
      while(num_items-- != 0)
         *dst-- = *src--;
      }

   // fill in the requested values
   for(size_t i=0; i<count; i++)
      ptr[index+i] = array[i];
   len = new_len;
   return true;
   }


/////////////////////////////////////////////////////////////////////////////
//
//  remove()
//

template<class TYPE>
bool Vector<TYPE>::remove(size_t index, size_t count)
   {
   if(index+count > len)
      return false;

   // cleanup items that are being removed
   for(size_t i=index; i<index+count; i++) {
       TYPE* p = ptr+i;
       p->~TYPE();
   }

   // move any remaining items down
   size_t num_items = len - index - count;
   memmove(&ptr[index], &ptr[index+count], num_items*sizeof(TYPE));
   len -= count;
   return true;
   }


/////////////////////////////////////////////////////////////////////////////
//
//  remove_all()
//

template<class TYPE>
void Vector<TYPE>::remove_all()
   {
   //cleanup any existing items
   for(size_t i=0; i<len; i++) {
       TYPE *p = ptr+i;
       p->~TYPE();
   }
   if(ptr != 0) 
       ulm_free(ptr);
   ptr = 0;
   len = 0;
   allocated = 0;
   }


/////////////////////////////////////////////////////////////////////////////
//
//  size()
//

template<class TYPE>
size_t Vector<TYPE>::size() const
   {
   return (size_t)len;
   }


/////////////////////////////////////////////////////////////////////////////
//
//  reserve()
//

template<class TYPE>
bool Vector<TYPE>::reserve(size_t new_size)
   {
   if(new_size <= allocated)
      return true;
   new_size = ((new_size/BLOCK_SIZE) + 1) * BLOCK_SIZE;
   TYPE *new_ptr = (TYPE*)ulm_malloc(new_size * sizeof(TYPE));
   if(new_ptr == 0) {
       ulm_err(("Vector<TYPE>::reserve(%d) ulm_malloc failed.", new_size));
       return false;
   }
   if(ptr != 0) {
      MEMCOPY_FUNC(ptr, new_ptr, len*sizeof(TYPE));
      ulm_free(ptr); 
   }
   ptr = new_ptr;
   allocated = new_size;
   return true;
   }


template<class TYPE>
bool Vector<TYPE>::size(size_t max_size)
   {
   // allocate more space if needed
   if(reserve(max_size) == false)
      return false;

   // if growing construct new items
   for(size_t i=len; i<max_size; i++)
       new(ptr+i) TYPE;

   // if shrinking delete unused items
   for(size_t i=max_size; i<len; i++) {
       TYPE *p = ptr+i;
       p->~TYPE();
   }
   len = max_size;
   return true;
   }

#endif
