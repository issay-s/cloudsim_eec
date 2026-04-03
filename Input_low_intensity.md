machine class:
{
        Number of machines: 24
        CPU type: X86
        Number of cores: 8
        Memory: 16384
        S-States: [120, 100, 100, 80, 40, 10, 0]
        P-States: [12, 8, 6, 4]
        C-States: [12, 3, 1, 0]
        MIPS: [1000, 800, 600, 400]
        GPUs: yes
}
task class:
{
        Start time: 60000
        End time : 2800000
        Inter arrival: 18000
        Expected runtime: 1600000
        Memory: 8
        VM type: LINUX
        GPU enabled: no
        SLA type: SLA2
        CPU type: X86
        Task type: WEB
        Seed: 220201
}
task class:
{
        Start time: 400000
        End time : 2600000
        Inter arrival: 25000
        Expected runtime: 2200000
        Memory: 12
        VM type: LINUX
        GPU enabled: no
        SLA type: SLA3
        CPU type: X86
        Task type: STREAM
        Seed: 220202
}
