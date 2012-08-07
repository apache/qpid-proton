package org.apache.qpid.proton.driver.impl;

import org.apache.qpid.proton.logging.LogHandler;

public class SystemOutLogger implements LogHandler
{       
    final String _prefix;

    public SystemOutLogger(String prefix)
    {
        _prefix = prefix;
    }
    
    @Override
    public boolean isTraceEnabled()
    {
        return true;
    }

    @Override
    public void trace(String message)
    {
        System.out.println(_prefix + message);
    }

    @Override
    public boolean isDebugEnabled()
    {
        return true;
    }

    @Override
    public void debug(String message)
    {
        System.out.println(_prefix + "DEBUG : " + message);
    }

    @Override
    public void debug(Throwable t, String message)
    {
        System.out.println(_prefix + "DEBUG : " + message);
        t.printStackTrace();
    }

    @Override
    public boolean isInfoEnabled()
    {
        return true;
    }

    @Override
    public void info(String message)
    {
        System.out.println(_prefix + "INFO : " + message);
    }

    @Override
    public void info(Throwable t, String message)
    {
        System.out.println(_prefix + "INFO : " + message);
        t.printStackTrace();
    }

    @Override
    public void warn(String message)
    {
        System.out.println(_prefix + "WARN : " + message);
    }

    @Override
    public void warn(Throwable t, String message)
    {
        System.out.println(_prefix + "INFO : " + message);
        t.printStackTrace();
    }

    @Override
    public void error(String message)
    {
        System.out.println(_prefix + "ERROR : " + message);
    }

    @Override
    public void error(Throwable t, String message)
    {
        System.out.println(_prefix + "INFO : " + message);
        t.printStackTrace();
    }
}
