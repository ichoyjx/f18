! Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
!
! Licensed under the Apache License, Version 2.0 (the "License");
! you may not use this file except in compliance with the License.
! You may obtain a copy of the License at
!
!     http://www.apache.org/licenses/LICENSE-2.0
!
! Unless required by applicable law or agreed to in writing, software
! distributed under the License is distributed on an "AS IS" BASIS,
! WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
! See the License for the specific language governing permissions and
! limitations under the License.

! Test compile-time analysis of shapes.

module m1
  integer(8), parameter :: a0s(:) = shape(3.14159)
  real :: a1(5,5,5)
  integer(8), parameter :: a1s(:) = shape(a1)
  integer(8), parameter :: a1ss(:) = shape(a1s)
  integer(8), parameter :: a1sss(:) = shape(a1ss)
  integer(8), parameter :: a1rs(:) = [rank(a1),rank(a1s),rank(a1ss),rank(a1sss)]
  integer(8), parameter :: a1n(:) = [size(a1),size(a1,1),size(a1,2)]
  integer(8), parameter :: a1sn(:) = [size(a1s),size(a1ss),size(a1sss)]
  integer(8), parameter :: ac1s(:) = shape([1])
  integer(8), parameter :: ac2s(:) = shape([1,2,3])
  integer(8), parameter :: ac3s(:) = shape([(1,j=1,4)])
  integer(8), parameter :: ac3bs(:) = shape([(1,j=4,1,-1)])
  integer(8), parameter :: ac4s(:) = shape([((j,k,j*k,k=1,3),j=1,4)])
  integer(8), parameter :: ac5s(:) = shape([((0,k=5,1,-2),j=9,2,-3)])
  integer(8), parameter :: rss(:) = shape(reshape([(0,j=1,90)], -[2,3]*(-[5_8,3_8])))
 contains
  subroutine subr(x,n1,n2)
    real, intent(in) :: x(:,:)
    integer, intent(in) :: n1(3), n2(:)
    real :: a(:,:,:)
    a = reshape(x,n1)
    a = reshape(x,n2(10:30:9)) ! fails if we can't figure out triplet shape
  end subroutine
end module m1
!Expect: m1.mod
! module m1
! integer(8),parameter::a0s(1_8:)=[Integer(8)::]
! real(4)::a1(1_8:5_8,1_8:5_8,1_8:5_8)
! integer(8),parameter::a1s(1_8:)=[Integer(8)::5_8,5_8,5_8]
! integer(8),parameter::a1ss(1_8:)=[Integer(8)::3_8]
! integer(8),parameter::a1sss(1_8:)=[Integer(8)::1_8]
! integer(8),parameter::a1rs(1_8:)=[Integer(8)::3_8,1_8,1_8,1_8]
! integer(8),parameter::a1n(1_8:)=[Integer(8)::125_8,5_8,5_8]
! integer(8),parameter::a1sn(1_8:)=[Integer(8)::3_8,1_8,1_8]
! integer(8),parameter::ac1s(1_8:)=[Integer(8)::1_8]
! integer(8),parameter::ac2s(1_8:)=[Integer(8)::3_8]
! integer(8),parameter::ac3s(1_8:)=[Integer(8)::4_8]
! integer(8),parameter::ac3bs(1_8:)=[Integer(8)::4_8]
! integer(8),parameter::ac4s(1_8:)=[Integer(8)::36_8]
! integer(8),parameter::ac5s(1_8:)=[Integer(8)::9_8]
! integer(8),parameter::rss(1_8:)=[Integer(8)::10_8,9_8]
! contains
! subroutine subr(x,n1,n2)
! real(4),intent(in)::x(1_8:,1_8:)
! integer(4),intent(in)::n1(1_8:3_8)
! integer(4),intent(in)::n2(1_8:)
! end
! end
