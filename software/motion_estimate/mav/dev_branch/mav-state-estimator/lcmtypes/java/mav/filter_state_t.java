/* LCM type definition class file
 * This file was automatically generated by lcm-gen
 * DO NOT MODIFY BY HAND!!!!
 */

package mav;
 
import java.io.*;
import java.nio.*;
import java.util.*;
import lcm.lcm.*;
 
public final class filter_state_t implements lcm.lcm.LCMEncodable
{
    public long utime;
    public double quat[];
    public int num_states;
    public double state[];
    public int num_cov_elements;
    public double cov[];
 
    public filter_state_t()
    {
        quat = new double[4];
    }
 
    public static final long LCM_FINGERPRINT;
    public static final long LCM_FINGERPRINT_BASE = 0x0a8b94bf1e099115L;
 
    static {
        LCM_FINGERPRINT = _hashRecursive(new ArrayList<Class>());
    }
 
    public static long _hashRecursive(ArrayList<Class> classes)
    {
        if (classes.contains(mav.filter_state_t.class))
            return 0L;
 
        classes.add(mav.filter_state_t.class);
        long hash = LCM_FINGERPRINT_BASE
            ;
        classes.remove(classes.size() - 1);
        return (hash<<1) + ((hash>>63)&1);
    }
 
    public void encode(DataOutput outs) throws IOException
    {
        outs.writeLong(LCM_FINGERPRINT);
        _encodeRecursive(outs);
    }
 
    public void _encodeRecursive(DataOutput outs) throws IOException
    {
        outs.writeLong(this.utime); 
 
        for (int a = 0; a < 4; a++) {
            outs.writeDouble(this.quat[a]); 
        }
 
        outs.writeInt(this.num_states); 
 
        for (int a = 0; a < this.num_states; a++) {
            outs.writeDouble(this.state[a]); 
        }
 
        outs.writeInt(this.num_cov_elements); 
 
        for (int a = 0; a < this.num_cov_elements; a++) {
            outs.writeDouble(this.cov[a]); 
        }
 
    }
 
    public filter_state_t(byte[] data) throws IOException
    {
        this(new LCMDataInputStream(data));
    }
 
    public filter_state_t(DataInput ins) throws IOException
    {
        if (ins.readLong() != LCM_FINGERPRINT)
            throw new IOException("LCM Decode error: bad fingerprint");
 
        _decodeRecursive(ins);
    }
 
    public static mav.filter_state_t _decodeRecursiveFactory(DataInput ins) throws IOException
    {
        mav.filter_state_t o = new mav.filter_state_t();
        o._decodeRecursive(ins);
        return o;
    }
 
    public void _decodeRecursive(DataInput ins) throws IOException
    {
        this.utime = ins.readLong();
 
        this.quat = new double[(int) 4];
        for (int a = 0; a < 4; a++) {
            this.quat[a] = ins.readDouble();
        }
 
        this.num_states = ins.readInt();
 
        this.state = new double[(int) num_states];
        for (int a = 0; a < this.num_states; a++) {
            this.state[a] = ins.readDouble();
        }
 
        this.num_cov_elements = ins.readInt();
 
        this.cov = new double[(int) num_cov_elements];
        for (int a = 0; a < this.num_cov_elements; a++) {
            this.cov[a] = ins.readDouble();
        }
 
    }
 
    public mav.filter_state_t copy()
    {
        mav.filter_state_t outobj = new mav.filter_state_t();
        outobj.utime = this.utime;
 
        outobj.quat = new double[(int) 4];
        System.arraycopy(this.quat, 0, outobj.quat, 0, 4); 
        outobj.num_states = this.num_states;
 
        outobj.state = new double[(int) num_states];
        if (this.num_states > 0)
            System.arraycopy(this.state, 0, outobj.state, 0, this.num_states); 
        outobj.num_cov_elements = this.num_cov_elements;
 
        outobj.cov = new double[(int) num_cov_elements];
        if (this.num_cov_elements > 0)
            System.arraycopy(this.cov, 0, outobj.cov, 0, this.num_cov_elements); 
        return outobj;
    }
 
}
