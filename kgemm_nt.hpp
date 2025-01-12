#ifndef KGEMM_NT_HPP
#define KGEMM_NT_HPP 1

#include "kroncommon.hpp"


//  -----------------------
//  NotransA and TransB case
//  C = alpha*A*transpose(B) + beta *C
//  -----------------------
template<typename T,typename Tc>
DEVICE_FUNCTION
void kgemm_nt2( int const mm, int const nn, int const kk, 
               T const alpha_in,
               T const * const A_,  int const ldA,
               T const * const B_,  int const ldB,
               T const beta_in,
               T * C_,  int const ldC)
{
#ifdef USE_LAMBDA
        auto min = []( int const x, int const y) {
                return(  (x < y) ? x : y );
        };
        auto max = []( int const x, int const y) {
                return(  (x > y) ? x : y );
        };
#else

#ifndef min
#define min(x,y)  (((x) < (y)) ? (x) : (y) )
#endif

#ifndef max
#define max(x,y)  (((x) > (y)) ? (x) : (y) )
#endif

#endif

        Tc const alpha = alpha_in;
        Tc const beta = beta_in;
	int constexpr nb = 2*32;
#ifdef USE_GPU
        // ---------------------------
        // use matlab 1 based indexing
        // ---------------------------

	int constexpr warpsize = WARPSIZE;
        int const nthreads = blockDim.x; 

        assert( blockDim.y == 1);
        assert( blockDim.z == 1);

        // -----------------------------------------
        // -----------------------------------------
        assert( (nthreads % warpsize) == 0);



	int const ij_start = threadIdx.x + 1;
	int const ij_size = nthreads;

#else


	int const ij_start = 1;
	int const ij_size = 1;
#endif



        //  ------------------------------------
        //  commonly  mm is large, but kk, nn are small
        //  ------------------------------------


#ifdef USE_LAMBDA
        auto A = [=] (int const ia,
                      int const ja) -> T const & {
                return( A_[ indx2f(ia,ja,ldA) ] );
        };

        auto B = [=] (int const ib,
                      int const jb) -> T const & {
                return( B_[ indx2f(ib,jb,ldB) ] );
        };

        auto C = [=] (int const ic,
                      int const jc) -> T& {
                return( C_[ indx2f(ic,jc,ldC) ] );
        };

#else

#define A(ia,ja)  A_[indx2f(ia,ja,ldA)]
#define B(ib,jb)  B_[indx2f(ib,jb,ldB)]
#define C(ic,jc)  C_[indx2f(ic,jc,ldC)]

#endif


        for(int jstart=1; jstart <= nn; jstart += nb) {
            int const jend = min(nn, jstart + nb-1);
            int const jsize = jend  - jstart + 1;

            for(int istart=1; istart <= mm;  istart += nb) {
                int const iend = min( mm, istart + nb-1);
                int const isize = iend - istart + 1;

                 SYNCTHREADS;

                    // ---------------------------
                    // perform matrix calculations
                    // ---------------------------

		    auto const inc_A = ldA;
		    auto const inc_B = ldB;

		    for(int ij0=ij_start-1; ij0 < (isize*jsize); ij0 += ij_size) {
			    int const  i = (ij0 % isize) + 1;
			    int const  j = (ij0 - (i-1))/isize + 1;
			    Tc cij = 0;
			    bool constexpr use_pointer = true;
			    if (use_pointer) {
				    int k = 1;
				    int ia = (istart-1) + i;
				    int ib = (jstart-1) + j;
				    T const * Ap = &(A(ia,k));
				    T const * Bp = &(B(ib,k));

#define case_code(kk)  { \
				       for(int k=0; k < kk; k++) { \
					    Tc const aik = (*Ap); \
					    Tc const bjk = (*Bp); \
					    cij += aik * bjk; \
					    Ap += inc_A; \
					    Bp += inc_B; \
				            }; \
				       break; \
				       }

				    switch(kk) {
				    case 1: case_code(1)
				    case 2: case_code(2)
				    case 3: case_code(3)
				    case 4: case_code(4)
				    case 5: case_code(5)
				    case 6: case_code(6)
				    case 7: case_code(7)
				    case 8: case_code(8)
			            default:
                                    case_code(kk);
				    };
			      }
			    else {
			      for(int k=1; k <= kk; k++) {
				Tc const aik = A( (istart-1) + i, k);  
				Tc const bjk = B( (jstart-1) + j, k);
				cij += aik * bjk;
			      };
			    };
                           // ------------------
                           // store results to C
                           // ------------------
                           int const ic = (istart-1) + i;
                           int const jc = (jstart-1) + j;
			   if (beta == 1) {
                             atomicAdd( &(C(ic,jc)), alpha*cij );
			     }
			   else if (beta == 0) {
		              C(ic,jc) = alpha * cij;
			      }
			   else {
			      C(ic,jc)  =  beta * C(ic,jc) + alpha*cij;
			   };

		    };

            }; // end istart
        }; // end jstart
}

template<typename T>
DEVICE_FUNCTION
void kgemm_nt( int const mm, int const nn, int const kk, 
               T const alpha_in,
               T const * const A_,  int const ldA,
               T const * const B_,  int const ldB,
               T const beta_in,
               T * C_,  int const ldC)
{
   kgemm_nt2<T,T>(
       mm, nn, kk,
       alpha_in, A_, ldA, B_, ldB,
       beta_in,  C_, ldC );
}


template<>
DEVICE_FUNCTION
void kgemm_nt( int const mm, int const nn, int const kk, 
               float const alpha_in,
               float const * const A_,  int const ldA,
               float const * const B_,  int const ldB,
               float const beta_in,
               float * C_,  int const ldC)
{
   kgemm_nt2<float,double>(
       mm, nn, kk,
       alpha_in, A_, ldA, B_, ldB,
       beta_in,  C_, ldC );
}





#undef min
#undef max
#undef A
#undef B
#undef C
#undef case_code

#endif
