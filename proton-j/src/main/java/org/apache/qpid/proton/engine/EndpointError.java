package org.apache.qpid.proton.engine;

/**
* Holds information about an endpoint error.
*/
public final class EndpointError
{

    private String name;
    private String description;

    public EndpointError(String name, String description)
    {
        this.name = name;
        this.description = description;
    }

    public EndpointError(String name)
    {
        this(name, null);
    }

    public String getName()
    {
        return name;
    }

    public String getDescription()
    {
        return description;
    }

    public String toString()
{
    if (description == null)
    {
        return name;
    }
    else
    {
        return String.format("%s -- %s", name, description);
    }
}
}
