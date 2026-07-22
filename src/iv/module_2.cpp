#include "module_2.hpp"
#include <cmath>
#include <algorithm>
#include <immintrin.h>
//#include <omp.h> //commented out all multithreading commands for now, can revisit later

namespace{

    //N(d1) & N(d2) calculation
    //Abramovitv-Stegun formula for prcise approximation
    inline double norm_cdf(double x) noexcept{

        const double b1 = 0.319381530;
        const double b2 =-0.356563782;
        const double b3 = 1.781477937;
        const double b4 =-1.821255978;
        const double b5 = 1.330274429;
        const double p  = 0.2316419;
        const double c  = 0.39894228;

        double a=std::abs(x);
        double t=1.0/(1.0+p*a);
        double m=1.0-c*std::exp(-x*x*0.5)*t*(b1+t*(b2+t*(b3+t*(b4+t*b5))));
        
        double res=(x>=0.0) ? m:1.0-m;
        res=(x>6.0) ? 1.0:res;
        res=(x<-6.0) ? 0.0:res;
        return res;
    }

    //N'(d1) & N'(d2)
    inline double norm_pdf(double x) noexcept{
        return (1.0/std::sqrt(2.0*M_PI))*std::exp(-x*x*0.5);
    }

    //Validating arbitrage bounds
    inline bool is_arbitrage(double target_price, double S, double K, double T, double r, double q, uint8_t is_call) noexcept{
        if(is_call){
            return target_price<std::max(0.0,S*std::exp(-q*T)-K*std::exp(-r*T));
        }
        return target_price<std::max(0.0,K*std::exp(-r*T)-S*std::exp(-q*T));
    }

    //Dynamic initial guess via Corrado-Miller formula, with fallback for D<0, to optimize Newton Raphson
    inline double compute_corrado_miller(double target_price, double S, double K, double T, double r, double q, uint8_t is_call) noexcept{
        double call_price=is_call ? target_price:target_price+S*std::exp(-q*T)-K*std::exp(-r*T);
        double D=std::pow(call_price-(S-K)/2.0,2.0)-(std::pow(S-K,2.0)/M_PI);
        return (D<0.0) ? 0.5 : (std::sqrt(2.0*M_PI)/(S+K))*(call_price-(S-K)/2.0 +std::sqrt(D));
    }

    //Helper function for Brent's Method, pricing residual utility f(sigma)=BSM price-market price
    inline double bsm_error(double vol, double target_price, double S, double K, double T, double r, double q, uint8_t is_call) noexcept{
        double d1=(std::log(S/K)+(r-q+vol*vol*0.5)*T)/(vol*std::sqrt(T));
        double d2=d1-vol*std::sqrt(T);
        double calc=is_call ? (S*std::exp(-q*T)*norm_cdf(d1)-K*std::exp(-r*T)*norm_cdf(d2)) :
                              (K*std::exp(-r*T)*norm_cdf(-d2)-S*std::exp(-q*T)*norm_cdf(-d1));
        return calc-target_price;
    }

    //Brent's Method, fallback if Newton-Raphson fails to converge
    double implied_vol_brent(double target_price, double S, double K, double T, double r, double q, uint8_t is_call) noexcept{
        double a=0.0001;
        double b=5.0;
        double fa=bsm_error(a, target_price, S, K, T, r, q, is_call);
        double fb=bsm_error(b, target_price, S, K, T, r, q, is_call);

        //Dynamically scale and expand bounds 
        if(fa*fb>0.0){
            for(int h=0; h<5; ++h){
                b*=2.0;
                a/=10.0;
                fa=bsm_error(a, target_price, S, K, T, r, q, is_call);
                fb=bsm_error(b, target_price, S, K, T, r, q, is_call);
                if(fa*fb<=0.0){
                    break;
                }
            }
        }

        if(fa*fb>0.0) return NAN;

        double c=a;
        double fc=fa;
        double d=b-a;
        double e=d;

        const double epsilon=1e-6;
        const double max_iter=100;

        for(int i=0; i<max_iter; ++i){
            if(std::abs(fc)<std::abs(fb)){
                a=b; b=c; c=a;
                fa=fb; fb=fc; fc=fa; 
            }

            double tol1=2.0*2.2204460492503131e-16*std::abs(b)+0.5*epsilon;
            double m=0.5*(c-b);

            if(std::abs(m)<=tol1 || fb==0.0){
                return b;
            }

            if(std::abs(e)>=tol1 && std::abs(fa)>std::abs(fb)){
                double s=fb/fa;
                double p,q_val;

                if(a==c){
                    p=2*m*s;
                    q_val=1.0-s;
                }
                else{
                    double q_ratio=fa/fc;
                    double r_ratio=fb/fc;
                    p=s*(2.0*m*q_ratio*(q_ratio-r_ratio)-(b-a)*(r_ratio-1.0));
                    q_val=(q_ratio-1.0)*(r_ratio-1)*(s-1.0);
                }

                if(p>0.0) q_val=-q_val;
                p=std::abs(p);

                double min1=3.0*m*q_val-std::abs(tol1*q_val);
                double min2=std::abs(e*q_val);

                if(2.0*p<((min1<min2)?min1:min2)){
                    e=d;
                    d=p/q_val;
                }
                else{
                    d=m;
                    e=m;
                }
            }
            else{
                d=m;
                e=m;
            }

            a=b;
            fa=fb;
            if(std::abs(d)>tol1){
                b+=d;
            }
            else{
                b+=(m>0.0)?tol1:-tol1;
            }
            fb=bsm_error(b, target_price, S, K, T, r, q, is_call);

            if((fb>0.0 && fc>0.0) || (fb<0.0 && fc<0.0)){
                c=a;
                fc=fa;
                d=b-a;
                e=d;
            }
        }
        return b;
    }
}

namespace ONITRO{
namespace Layer3{
namespace Volatility{

    // Primary Execution Interface: Vectorized IV pipeline
    // Uses cache-aligned L1 blocking (1024 chunks) and SIMD optimization passes
    void iv_chain_bsm_simd(const HomogenousOptionBatch& batch, 
                           std::size_t N,
                           double epsilon,
                           std::size_t max_iter,
                           double* __restrict__ out_ivs) noexcept{
        
        //#pragma omp parallel for if(N>10000) schedule(dynamic,1024)// Multithreading command

        // Block based data chunking for cache architecture
        for(std::size_t chunk=0; chunk<N; chunk+=1024){
            
            const std::size_t limit=std::min(std::size_t(1024),N-chunk);

            // 64-Byte Alignment enforces flawless AVX-512/AVX2 cache line loads
            alignas(64) double sigma_n[1024];
            alignas(64) uint8_t active_mask[1024];
            alignas(64) uint8_t fallback_mask[1024];

            alignas(64) double S_local[1024], K_local[1024], T_local[1024];
            alignas(64) double r_local[1024], q_local[1024], tgt_local[1024];
            alignas(64) uint8_t is_call_local[1024];

            // Pipeline Stage 1: Local memory ingestion, boundary profiling, and initial seed estimation
            #pragma omp simd
            for(std::size_t i=0; i<limit; ++i){
                
                std::size_t idx=chunk+i;

                S_local[i]=batch.spots[idx];
                K_local[i]=batch.strikes[idx];
                T_local[i]=batch.maturities[idx];
                r_local[i]=batch.rates[idx];
                q_local[i]=batch.div_yields[idx];
                tgt_local[i]=batch.target_prices[idx];
                is_call_local[i]=batch.is_call[idx];

                if(tgt_local[i]>1e-6 && T_local[i]>1e-6 && S_local[i]>1e-6 && K_local[i]>1e-6 && !is_arbitrage(tgt_local[i], S_local[i], K_local[i], T_local[i], r_local[i], q_local[i], is_call_local[i]))[[likely]]{
                    double cm_vol=compute_corrado_miller(tgt_local[i], S_local[i], K_local[i], T_local[i], r_local[i], q_local[i], is_call_local[i]);
                    sigma_n[i]=(cm_vol<0.0001) ? 0.0001:cm_vol;
                    active_mask[i]=1;     
                    fallback_mask[i]=0;       
                }
                else{
                    out_ivs[idx]=NAN;
                    active_mask[i]=0;
                    fallback_mask[i]=0;
                }
            }

            // Pipeline Stage 2: Massively parallelized Newton-Raphson convergence refinement loop
            for(std::size_t iter=0; iter<max_iter; ++iter){
                int active_count=0;
               
                #pragma omp simd reduction(+:active_count)
                for(std::size_t i=0; i<limit; ++i){
                    if(active_mask[i]){
                        std::size_t idx=chunk+i;
                        const double S=S_local[i], T=T_local[i], K=K_local[i];
                        const double r=r_local[i], q=q_local[i], vol=sigma_n[i];
                        const uint8_t is_call=is_call_local[i];

                        const double d1=(std::log(S/K)+(r-q+vol*vol*0.5)*T)/(vol*std::sqrt(T));
                        const double d2=d1-vol*std::sqrt(T);
                        double calc=is_call ? (S*std::exp(-q*T)*norm_cdf(d1)-K*std::exp(-r*T)*norm_cdf(d2)):
                                                       (K*std::exp(-r*T)*norm_cdf(-d2)-S*std::exp(-q*T)*norm_cdf(-d1));
                        double vega=S*std::exp(-q*T)*norm_pdf(d1)*std::sqrt(T);
                        double f_val=calc-batch.target_prices[idx];
                        
                        if((std::abs(f_val))<epsilon){
                            out_ivs[idx]=vol;
                            active_mask[i]=0;
                            fallback_mask[i]=0;
                        }
                        else if(vega>1e-8){
                            double step=f_val/vega;
                            double next_sigma=sigma_n[i]-step;
                            if(next_sigma<0.0001) next_sigma=0.0001;
                            if(next_sigma>10.0) next_sigma=10.0;
                            sigma_n[i]=next_sigma;
                        }
                        else{
                            active_mask[i]=0;
                            fallback_mask[i]=1;
                        }
                    }
                    active_count+=active_mask[i];
                }
                if(active_count==0) break;
            }

            // Flag remaining elements failing Newton-Raphson limits for fallback resolution
            #pragma omp simd
            for(std::size_t i=0; i<limit; ++i){
                if (active_mask[i]){
                    fallback_mask[i]=1;
                }
            }

            // Pipeline Stage 3: Direct execution of exact Brent solver for slow-converging/volatile profiles
            #pragma omp simd
            for(std::size_t i=0; i<limit; ++i){
                if(fallback_mask[i]){
                    std::size_t idx=chunk+i;
                    out_ivs[idx]=implied_vol_brent(tgt_local[i], S_local[i], K_local[i],
                        T_local[i], r_local[i], q_local[i], is_call_local[i]); 
                }
            }
        }
    }    

    // Complete Execution Pipeline: Synchronous estimation of Implied Volatilities and all structural Greeks.
    // Evaluates selected risk exposures concurrently using analytical formulas following convergence.
    void iv_and_greeks_chain_simd(const HomogenousOptionBatch& batch,
                                  std::size_t N,
                                  double epsilon,
                                  std::size_t max_iter,
                                  uint8_t output_mask,
                                  double* __restrict__ out_ivs,
                                  double* __restrict__ out_deltas,
                                  double* __restrict__ out_gammas,
                                  double* __restrict__ out_vegas,
                                  double* __restrict__ out_thetas,
                                  double* __restrict__ out_rhos)noexcept{
        
        //#pragma omp parallel for if(N>10000) schedule(dynamic,1024)// multithreading command
        for(std::size_t chunk=0; chunk<N; chunk+=1024){
            
            const std::size_t limit=std::min(std::size_t(1024),N-chunk);

            alignas(64) double sigma_n[1024];
            alignas(64) uint8_t active_mask[1024];
            alignas(64) uint8_t fallback_mask[1024];

            alignas(64) double S_local[1024], K_local[1024], T_local[1024];
            alignas(64) double r_local[1024], q_local[1024], tgt_local[1024];
            alignas(64) uint8_t is_call_local[1024];

            // Pipeline Stage 1: Data caching, sanity profiling, and boundary tracking
            #pragma omp simd
            for(std::size_t i=0; i<limit; ++i){
                
                std::size_t idx=chunk+i;
                
                S_local[i]=batch.spots[idx];
                K_local[i]=batch.strikes[idx];
                T_local[i]=batch.maturities[idx];
                r_local[i]=batch.rates[idx];
                q_local[i]=batch.div_yields[idx];
                tgt_local[i]=batch.target_prices[idx];
                is_call_local[i]=batch.is_call[idx];

                if(tgt_local[i]>1e-6 && T_local[i]>1e-6 && S_local[i]>1e-6 && K_local[i]>1e-6 && !is_arbitrage(tgt_local[i], S_local[i], K_local[i], T_local[i], r_local[i], q_local[i], is_call_local[i]))[[likely]]{
                    double cm_vol=compute_corrado_miller(tgt_local[i], S_local[i], K_local[i], T_local[i], r_local[i], q_local[i], is_call_local[i]);
                    sigma_n[i]=(cm_vol<0.0001) ? 0.0001:cm_vol;
                    active_mask[i]=1;      
                    fallback_mask[i]=0;      
                }
                else{
                    if(output_mask & WANT_IV) out_ivs[idx]=NAN;
                    if(output_mask & WANT_DELTA) out_deltas[idx]=NAN;
                    if(output_mask & WANT_THETA) out_thetas[idx]=NAN;
                    if(output_mask & WANT_VEGA) out_vegas[idx]=NAN;
                    if(output_mask & WANT_GAMMA) out_gammas[idx]=NAN;
                    if(output_mask & WANT_RHO) out_rhos[idx]=NAN;
                    active_mask[i]=0;
                    fallback_mask[i]=0;
                }
            }
            
            // Pipeline Stage 2: Newton-Raphson iteration loop accompanied by simultaneous Greek calculation
            for(std::size_t iter=0; iter<max_iter; ++iter){
                int active_count=0;

                #pragma omp simd reduction(+:active_count)
                for(std::size_t i=0; i<limit; ++i){
                    if(active_mask[i]){
                        std::size_t idx=chunk+i;

                        const double S=S_local[i], T=T_local[i], K=K_local[i];
                        const double r=r_local[i], q=q_local[i], vol=sigma_n[i];
                        const uint8_t is_call=is_call_local[i];

                        const double d1=(std::log(S/K)+(r-q+vol*vol*0.5)*T)/(vol*std::sqrt(T));
                        const double d2=d1-vol*std::sqrt(T);
                        double calc=is_call?(S*std::exp(-q*T)*norm_cdf(d1)-K*std::exp(-r*T)*norm_cdf(d2)):
                                            (K*std::exp(-r*T)*norm_cdf(-d2)-S*std::exp(-q*T)*norm_cdf(-d1));
                        double vega=S*std::exp(-q*T)*norm_pdf(d1)*std::sqrt(T);
                        double f_val=calc-tgt_local[i];

                        if(std::abs(f_val)<epsilon){
                            if(output_mask & WANT_IV){
                                out_ivs[idx]=vol;
                            }
                            if(output_mask & WANT_DELTA){
                                out_deltas[idx]=is_call ? (std::exp(-q*T)*norm_cdf(d1))
                                                        : (std::exp(-q*T)*(norm_cdf(d1)-1.0));
                            }
                            if(output_mask & WANT_GAMMA){
                                out_gammas[idx]=(std::exp(-q*T)*norm_pdf(d1))/(S*vol*std::sqrt(T));
                            }
                            if(output_mask & WANT_VEGA){
                                out_vegas[idx]=vega;
                            }
                            if(output_mask & WANT_THETA){
                                double theta_c=-(S*std::exp(-q*T)*norm_pdf(d1)*vol)/(2.0*std::sqrt(T))
                                               -r*K*std::exp(-r*T)*norm_cdf(d2)+q*S*std::exp(-q*T)*norm_cdf(d1);
                                out_thetas[idx]=is_call ? theta_c:theta_c+r*K*std::exp(-r*T)-q*S*std::exp(-q*T);               
                            }
                            if(output_mask & WANT_RHO){
                                out_rhos[idx]=is_call ? K*T*std::exp(-r*T)*norm_cdf(d2):-K*T*std::exp(-r*T)*norm_cdf(-d2);
                            }

                            active_mask[i]=0;
                            fallback_mask[i]=0;
                        }
                        else if(vega>1e-8){
                            double step=f_val/vega;
                            double next_sigma=sigma_n[i]-step;
                            if(next_sigma<0.0001) next_sigma=0.0001;
                            if(next_sigma>10.0) next_sigma=10.0;
                            sigma_n[i]=next_sigma;
                        }
                        else{
                            active_mask[i]=0;
                            fallback_mask[i]=1;
                        }
                    }
                    active_count+=active_mask[i];
                }
                if(active_count==0) break;
            }  
 
            #pragma omp simd
            for(std::size_t i=0; i<limit; ++i){
                if (active_mask[i]){
                    fallback_mask[i]=1;
                }
            }

            // Pipeline Stage 3: Brent solver evaluation pass for items requiring backup
            #pragma omp simd
            for(std::size_t i=0; i<limit; ++i){
                if(fallback_mask[i]){
                    std::size_t idx=chunk+i;
                    const double S=S_local[i], T=T_local[i], K=K_local[i];
                    const double r=r_local[i], q=q_local[i];
                    const uint8_t is_call=is_call_local[i];

                    double brent_vol=implied_vol_brent(tgt_local[i],S,K,T,r,q,is_call);

                    if(output_mask & WANT_IV){
                        out_ivs[idx]=brent_vol;
                    }

                    if(brent_vol!=brent_vol){
                        if(output_mask & WANT_DELTA) out_deltas[idx]=NAN;
                        if(output_mask & WANT_GAMMA) out_gammas[idx]=NAN;
                        if(output_mask & WANT_VEGA) out_vegas[idx]=NAN;
                        if(output_mask & WANT_THETA) out_thetas[idx]=NAN;
                        if(output_mask & WANT_RHO) out_rhos[idx]=NAN;
                    }

                    else if(output_mask & (WANT_DELTA | WANT_GAMMA | WANT_VEGA | WANT_THETA | WANT_RHO)){
                        const double d1=(std::log(S/K)+(r-q+0.5*brent_vol*brent_vol)*T)/(brent_vol*std::sqrt(T));
                        const double d2=d1-brent_vol*std::sqrt(T);

                        if(output_mask & WANT_DELTA){
                            out_deltas[idx]=is_call ? std::exp(-q*T)*norm_cdf(d1):std::exp(-q*T)*(norm_cdf(d1)-1.0);
                        }
                        if(output_mask & WANT_VEGA){
                            out_vegas[idx]=S*std::exp(-q*T)*norm_pdf(d1)*std::sqrt(T);
                        }
                        if(output_mask & WANT_GAMMA){
                            out_gammas[idx]=(std::exp(-q*T)*norm_pdf(d1))/(S*brent_vol*std::sqrt(T));
                        }
                        if(output_mask & WANT_THETA){
                            double theta_c=-(S*std::exp(-q*T)*norm_pdf(d1)*brent_vol)/(2.0*std::sqrt(T))
                                            -r*K*std::exp(-r*T)*norm_cdf(d2)+q*S*std::exp(-q*T)*norm_cdf(d1);
                            out_thetas[idx]=is_call ? theta_c:theta_c+r*K*std::exp(-r*T)-q*S*std::exp(-q*T);
                        }
                        if(output_mask & WANT_RHO){
                            out_rhos[idx]=is_call ? K*T*std::exp(-r*T)*norm_cdf(d2):-K*T*std::exp(-r*T)*norm_cdf(-d2);
                        }
                    }
                }
            }
        }                      
    }
                            
}
}
}